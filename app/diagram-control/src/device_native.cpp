/** @file device_native.cpp
 * @brief Python機器APIを共有ドライバーへ接続する。VMとの型・ID変換はdevice_api.generated.cppが担当する。
 * @details Displayの描画・I2C転送はlib/display、VMハンドル・分割文字列の寿命と結果変換は本ファイルが担当する。
 */
#include "device_native.h"
#include "config.h"
#include <algorithm>
#include <cmath>

namespace {
    /** @brief 教材の表示設定を共有ライブラリーへ渡す。@return config.hの画面と100kHzの共有バス設定。 */
    uroa::Ssd1306Display::Config displayConfig() {
        uroa::Ssd1306Display::Config config;
        config.width = DISPLAY_WIDTH;
        config.height = DISPLAY_HEIGHT;
        config.address = DISPLAY_ADDRESS;
        config.i2cClockHz = 100000;
        return config;
    }
}

/** @brief loop()が共有する機器を借用する。@param wire I2C。@param servos サーボ。@param imu IMU。@param distance 距離の受渡し。 */
DeviceNative::DeviceNative(TwoWire &wire, uroa::Pca9685Servos &servos, uroa::Mpu6050 &imu, DistanceCallbacks distance)
    : servos_(servos), imu_(imu), distance_(distance), display_(wire, displayConfig()) {
}

/** @brief VMが有効化したCHと距離要求だけを停止する。他の通信によるSTREAMは保持する。
 * @details ハンドルは最大32個。reset後も連番を戻さず、古いハンドルを別の機器へ再利用しない。
 */
void DeviceNative::reset() {
    for (Object &object : objects_) {
        if (object.handle != 0 && object.kind == Kind::Servo && object.ownsOutput) {
            servos_.disable(object.channel);
        }
        object = Object{};
    }
    if (distance_.enable) {
        distance_.enable(false);
    }
    display_.reset();
    textOffset_ = 0;
    serialOffset_ = 0;
}

/** @brief 生存ハンドルと種別を照合する。@param handle 対象。@param kind 必要種別。@return 情報またはnull。 */
DeviceNative::Object *DeviceNative::find(uint32_t handle, Kind kind) {
    for (Object &object : objects_) {
        if (handle != 0 && object.handle == handle && object.kind == kind) {
            return &object;
        }
    }
    return nullptr;
}

/** @brief 空き領域に新規ハンドルを割り当てる。@param kind 種別。@param result 出力。@return 実行結果。 */
DeviceNative::Error DeviceNative::create(Kind kind, uint32_t &result) {
    if (nextHandle_ == 0) {
        return Error::Limit;
    }
    for (Object &object : objects_) {
        if (object.handle == 0) {
            object.kind = kind;
            object.handle = nextHandle_++;
            result = object.handle;
            return Error::None;
        }
    }
    return Error::Limit;
}

/** @brief CH・モードを保持し、出力は開始しない。@param channel CH0〜15。@param mode 0=角度、1=速度。@param result ハンドル。@return 実行結果。
 * @details 同一実行内の同一CHの重複確保は拒否する。PCAの初期化はsetup()の結果を再利用する。
 */
DeviceNative::Error DeviceNative::Servo_construct(int32_t channel, int32_t mode, uint32_t &result) {
    if (channel < 0 || channel >= 16 || mode < 0 || mode > 1) {
        return Error::Operand;
    }
    for (const Object &object : objects_) {
        if (object.handle != 0 && object.kind == Kind::Servo && object.channel == channel) {
            return Error::Operand;
        }
    }
    if (!i2cReady || !servos_.available) {
        return Error::NativeFailure;
    }
    const Error status = create(Kind::Servo, result);
    if (status == Error::None) {
        Object *object = find(result, Kind::Servo);
        object->channel = uint8_t(channel);
        object->mode = uint8_t(mode);
    }
    return status;
}

/** @brief 角度90度または速度0%で有効化する。@param receiver ハンドル。@return 実行結果。
 * @details 同じモードですでに有効なら現在の指令を保つ。別モードの有効CHは上書きしない。
 */
DeviceNative::Error DeviceNative::Servo_enable(uint32_t receiver) {
    Object *object = find(receiver, Kind::Servo);
    if (!object) {
        return Error::Operand;
    }
    const auto &state = servos_.states[object->channel];
    const auto mode = object->mode == 0 ? uroa::Pca9685Servos::Mode::Angle : uroa::Pca9685Servos::Mode::Speed;
    if (state.armed) {
        if (state.mode != mode || !servos_.available || !object->ownsOutput) {
            return Error::NativeFailure;
        }
        // このオブジェクトが有効化した出力だけを再利用し、他の通信のCHは取り込まない。
        return Error::None;
    }
    const auto status = object->mode == 0 ? servos_.armAngle(object->channel, 90) : servos_.armSpeed(object->channel);
    if (status != uroa::Pca9685Servos::Result::Ok) {
        return Error::NativeFailure;
    }
    object->ownsOutput = true;
    return Error::None;
}

/** @brief PWMを無効化する。@param receiver ハンドル。@return 実行結果。 */
DeviceNative::Error DeviceNative::Servo_disable(uint32_t receiver) {
    Object *object = find(receiver, Kind::Servo);
    if (!object) {
        return Error::Operand;
    }
    const auto status = servos_.disable(object->channel);
    object->ownsOutput = false;
    return status == uroa::Pca9685Servos::Result::Ok ? Error::None : Error::NativeFailure;
}

/** @brief 有効な角度CHへ即時指令する。@param receiver ハンドル。@param angle 度、設定済み60〜120度の範囲。@return 実行結果。 */
DeviceNative::Error DeviceNative::Servo_set_angle(uint32_t receiver, float angle) {
    Object *object = find(receiver, Kind::Servo);
    if (!object || object->mode != 0 || !object->ownsOutput || !std::isfinite(angle)) {
        return Error::Operand;
    }
    return servos_.moveAngle(object->channel, angle, 0) == uroa::Pca9685Servos::Result::Ok ? Error::None : Error::NativeFailure;
}

/** @brief 有効な速度CHへ指令する。@param receiver ハンドル。@param speed −100〜100%。@return 実行結果。 */
DeviceNative::Error DeviceNative::Servo_set_speed(uint32_t receiver, float speed) {
    Object *object = find(receiver, Kind::Servo);
    if (!object || object->mode != 1 || !object->ownsOutput || !std::isfinite(speed)) {
        return Error::Operand;
    }
    return servos_.setSpeed(object->channel, speed) == uroa::Pca9685Servos::Result::Ok ? Error::None : Error::NativeFailure;
}

/** @brief 初期化済みIMUを利用する。@param result ハンドル。@return 実行結果。
 * @details 読取失敗後は新しいIMU()で再探索できる。Wire自体の開始はsetup()だけが担当する。
 */
DeviceNative::Error DeviceNative::IMU_construct(uint32_t &result) {
    if (!i2cReady || (!imu_.available && !imu_.begin())) {
        return Error::NativeFailure;
    }
    return create(Kind::IMU, result);
}

/** @brief 加速度m/s²と角速度°/sを値として返す。@param receiver ハンドル。@param result acc/gyro。@return 実行結果。
 * @details MPU6050の±2g/±250°/s設定と既存diagramの変換式に合わせる。IMUValueは参照共有しない。
 */
DeviceNative::Error DeviceNative::IMU_get_value(uint32_t receiver, device_api::IMUValue &result) {
    if (!find(receiver, Kind::IMU)) {
        return Error::Operand;
    }
    uint8_t raw[14];
    if (!imu_.readRaw(raw)) {
        return Error::NativeFailure;
    }
    float values[6];
    for (size_t index = 0; index < 6; ++index) {
        const size_t offset = index < 3 ? index * 2 : index * 2 + 2;
        const uint16_t bits = (uint16_t(raw[offset]) << 8) | raw[offset + 1];
        const int32_t signedValue = bits >= 0x8000 ? int32_t(bits) - 65536 : int32_t(bits);
        values[index] = index < 3 ? float(signedValue) / 16384.0f * 9.80665f : float(signedValue) / 131.0f;
    }
    result.acc = {values[0], values[1], values[2]};
    result.gyro = {values[3], values[4], values[5]};
    return Error::None;
}

/** @brief VMの距離測定要求を開始する。@param result ハンドル。@return 実行結果。
 * @details GPIOと測定は既存の専用タスクだけが操作する。STREAMの登録は必要ない。
 */
DeviceNative::Error DeviceNative::UltrasonicDistanceSensor_construct(uint32_t &result) {
    if (!distance_.ready || !distance_.enable || !distance_.read || !distance_.ready()) {
        return Error::NativeFailure;
    }
    const Error status = create(Kind::Distance, result);
    if (status == Error::None) {
        distance_.enable(true);
    }
    return status;
}

/** @brief キュー内の最新距離をmmで返す。@param receiver ハンドル。@param result mm。@return 実行結果またはNativePending。
 * @details 初回の測定待ちはVMだけを中断し、loop()の停止・通信処理を継続する。失敗値を数値として返さない。
 */
DeviceNative::Error DeviceNative::UltrasonicDistanceSensor_get_distance(uint32_t receiver, float &result) {
    if (!find(receiver, Kind::Distance)) {
        return Error::Operand;
    }
    return distance_.read(result);
}

/** @brief 表示ライブラリーの結果をVMへ変換する。@param result ドライバーの結果。@return VMの結果。 */
DeviceNative::Error DeviceNative::displayError(uroa::Ssd1306Display::Result result) {
    using Result = uroa::Ssd1306Display::Result;
    switch (result) {
        case Result::Ok:
            return Error::None;
        case Result::Pending:
            return Error::NativePending;
        case Result::InvalidArgument:
            return Error::Operand;
        case Result::NoMemory:
            return Error::Memory;
        case Result::NotInitialized:
        case Result::I2cError:
            return Error::NativeFailure;
    }
    return Error::NativeFailure;
}

/** @brief SSD1306の共有バッファを準備する。@param result ハンドル。@return 実行結果。
 * @details config.hの寸法・I2Cアドレスを使う。1実行の初回だけ白・座標0・空バッファへ戻す。
 * Wire.beginは行わず、100kHzを保つ。複数Displayは1台の表示バッファを共有する。
 * resetで転送を中止し、次回のDisplay()で再初期化する。画面はdisplay()まで前回の表示を保つ。
 */
DeviceNative::Error DeviceNative::Display_construct(uint32_t &result) {
    if (!i2cReady) {
        return Error::NativeFailure;
    }
    if (!display_.available) {
        const Error status = displayError(display_.begin());
        if (status != Error::None) {
            return status;
        }
    }
    return create(Kind::Display, result);
}

/** @brief 後続の文字色を設定する。@param receiver ハンドル。@param color 0=黒、1=白、2=反転。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_set_text_color(uint32_t receiver, int32_t color) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.setTextColor(color));
}

/** @brief 文字の開始位置を設定する。@param receiver ハンドル。@param x 横。@param y 縦。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_set_cursor(uint32_t receiver, int32_t x, int32_t y) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.setCursor(x, y));
}

/** @brief バッファへ文字を書く。@param receiver ハンドル。@param text 長さ付きバイト列。@return 実行結果またはNativePending。
 * @details 同じ文字列を保持したまま1回64バイトまででloopへ戻る。改行は付けない。標準GFXフォントを使い、日本語フォント・UTF-8変換は行わない。
 */
DeviceNative::Error DeviceNative::Display_print(uint32_t receiver, device_api::StringView text) {
    if (!find(receiver, Kind::Display) || textOffset_ > text.length || (!text.data && text.length != 0)) {
        return Error::Operand;
    }
    const size_t count = std::min(size_t(64), text.length - textOffset_);
    const char *data = text.data ? text.data + textOffset_ : nullptr;
    const Error status = displayError(display_.print(data, count));
    if (status != Error::None) {
        return status;
    }
    textOffset_ += count;
    if (textOffset_ != text.length) {
        return Error::NativePending;
    }
    textOffset_ = 0;
    return Error::None;
}

/** @brief バッファへ線を描く。@param receiver ハンドル。@param x1 始点X。@param y1 始点Y。@param x2 終点X。@param y2 終点Y。@param color 色。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_draw_line(
    uint32_t receiver, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.drawLine(x1, y1, x2, y2, color));
}

/** @brief バッファへ矩形を描く。@param receiver ハンドル。@param x 左。@param y 上。@param width 幅0〜1024。@param height 高さ0〜1024。@param color 色。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_draw_rect(
    uint32_t receiver, int32_t x, int32_t y, int32_t width, int32_t height, int32_t color) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.drawRect(x, y, width, height, color));
}

/** @brief バッファへ円を描く。@param receiver ハンドル。@param x 中心X。@param y 中心Y。@param radius 半径0〜1024。@param color 色。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_draw_circle(
    uint32_t receiver, int32_t x, int32_t y, int32_t radius, int32_t color) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.drawCircle(x, y, radius, color));
}

/** @brief バッファの円を塗る。@param receiver ハンドル。@param x 中心X。@param y 中心Y。@param radius 半径0〜1024。@param color 色。@return 実行結果。 */
DeviceNative::Error DeviceNative::Display_fill_circle(
    uint32_t receiver, int32_t x, int32_t y, int32_t radius, int32_t color) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.fillCircle(x, y, radius, color));
}

/** @brief 描画バッファを画面へ転送する。@param receiver ハンドル。@return 実行結果またはNativePending。
 * @details lib/displayがSSD1306へ16バイトずつ送り、各loopでSTOPを処理できるようにする。I2C失敗はNativeFailure。
 */
DeviceNative::Error DeviceNative::Display_display(uint32_t receiver) {
    if (!find(receiver, Kind::Display)) {
        return Error::Operand;
    }
    return displayError(display_.displayStep());
}

/** @brief 長さ付き文字列と改行をSerialへ送る。@param text UTF-8バイト列。@return 実行結果またはNativePending。
 * @details 空き送信バッファ分、最大64バイトずつ書く。NUL終端に依存せず、送信待ちでloopを占有しない。
 */
DeviceNative::Error DeviceNative::function_print(device_api::StringView text) {
    // USB CDCの未接続時は出力を捨て、BLEで動かすプログラムを待たせない。
    if (!Serial) {
        serialOffset_ = 0;
        return Error::None;
    }
    if (serialOffset_ > text.length) {
        return Error::Operand;
    }
    const int available = Serial.availableForWrite();
    if (available <= 0) {
        return Error::NativePending;
    }
    const size_t count = std::min(std::min(size_t(64), size_t(available)), text.length - serialOffset_);
    if (count != 0) {
        serialOffset_ += Serial.write(reinterpret_cast<const uint8_t *>(text.data + serialOffset_), count);
    }
    if (serialOffset_ != text.length || Serial.availableForWrite() <= 0) {
        return Error::NativePending;
    }
    if (Serial.write(uint8_t('\n')) != 1) {
        return Error::NativePending;
    }
    serialOffset_ = 0;
    return Error::None;
}
