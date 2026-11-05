/** @file device_native.h
 * @brief 生成スタブから呼ぶ機器操作。各メソッドの規約はdevice_native.cppの処理付近を参照。
 */
#pragma once
#include "device_api.generated.h"
#include <Adafruit_SSD1306.h>
#include <uroa_imu.h>
#include <uroa_servo.h>

/** @brief setup/loop()で既存ドライバーを共有する、Python機器APIのバックエンド。 */
class DeviceNative : public device_api::Backend {
    public:
    using Error = BytecodeVM::Error;
    /** @brief 専用測定タスクとの受渡し。すべてloop()から呼ぶ。 */
    struct DistanceCallbacks {
        bool (*ready)(); ///< GPIO設定とタスクの利用可否。センサー応答の検出ではない。
        void (*enable)(bool active); ///< VMの測定要求だけを開始・解除する。
        Error (*read)(float &millimetres); ///< 未測定ならNativePending、測定失敗ならNativeFailure。
    };
    bool i2cReady = false; ///< setup()がWire.begin()の結果を設定する。以降は参照だけ。
    /** @brief 既存のドライバーと測定要求を借用する。@param wire 開始済みバス。@param servos サーボ。@param imu IMU。@param distance 距離の受渡し。 */
    DeviceNative(TwoWire &wire, uroa::Pca9685Servos &servos, uroa::Mpu6050 &imu, DistanceCallbacks distance);
    /** @brief VMの出力を停止し、機器ハンドルを無効化する。 */
    void reset() override;
    /** @brief サーボのCH・モードを保持する。@param channel CH0〜15。@param mode 0=角度、1=速度。@param result ハンドル。@return 実行結果。 */
    Error Servo_construct(int32_t channel, int32_t mode, uint32_t &result) override;
    /** @brief 選択モードで出力を有効にする。@param receiver ハンドル。@return 実行結果。 */
    Error Servo_enable(uint32_t receiver) override;
    /** @brief PWMを無効にする。@param receiver ハンドル。@return 実行結果。 */
    Error Servo_disable(uint32_t receiver) override;
    /** @brief 度単位の角度を指令する。@param receiver ハンドル。@param angle 度。@return 実行結果。 */
    Error Servo_set_angle(uint32_t receiver, float angle) override;
    /** @brief 連続回転サーボへ速度を指令する。@param receiver ハンドル。@param speed −100〜100%。@return 実行結果。 */
    Error Servo_set_speed(uint32_t receiver, float speed) override;
    /** @brief IMUの利用を開始する。@param result ハンドル。@return 実行結果。 */
    Error IMU_construct(uint32_t &result) override;
    /** @brief IMUの6成分を値コピーで返す。@param receiver ハンドル。@param result 加速度・角速度。@return 実行結果。 */
    Error IMU_get_value(uint32_t receiver, device_api::IMUValue &result) override;
    /** @brief 距離の測定要求を開始する。@param result ハンドル。@return 実行結果。 */
    Error UltrasonicDistanceSensor_construct(uint32_t &result) override;
    /** @brief 最新の距離を返す。@param receiver ハンドル。@param result mm。@return 実行結果またはNativePending。 */
    Error UltrasonicDistanceSensor_get_distance(uint32_t receiver, float &result) override;
    /** @brief SSD1306の利用を開始する。@param result ハンドル。@return 実行結果。 */
    Error Display_construct(uint32_t &result) override;
    /** @brief 文字色を設定する。@param receiver ハンドル。@param color 0=黒、1=白、2=反転。@return 実行結果。 */
    Error Display_set_text_color(uint32_t receiver, int32_t color) override;
    /** @brief 文字の開始位置を設定する。@param receiver ハンドル。@param x 横座標。@param y 縦座標。@return 実行結果。 */
    Error Display_set_cursor(uint32_t receiver, int32_t x, int32_t y) override;
    /** @brief 表示バッファへ文字を書く。@param receiver ハンドル。@param text UTF-8バイト列。@return 実行結果またはNativePending。 */
    Error Display_print(uint32_t receiver, device_api::StringView text) override;
    /** @brief 線を描く。@param receiver ハンドル。@param x1 始点X。@param y1 始点Y。@param x2 終点X。@param y2 終点Y。@param color 色。@return 実行結果。 */
    Error Display_draw_line(uint32_t receiver, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color) override;
    /** @brief 矩形を描く。@param receiver ハンドル。@param x 左。@param y 上。@param width 幅。@param height 高さ。@param color 色。@return 実行結果。 */
    Error Display_draw_rect(uint32_t receiver, int32_t x, int32_t y, int32_t width, int32_t height, int32_t color) override;
    /** @brief 円を描く。@param receiver ハンドル。@param x 中心X。@param y 中心Y。@param radius 半径。@param color 色。@return 実行結果。 */
    Error Display_draw_circle(uint32_t receiver, int32_t x, int32_t y, int32_t radius, int32_t color) override;
    /** @brief 円を塗る。@param receiver ハンドル。@param x 中心X。@param y 中心Y。@param radius 半径。@param color 色。@return 実行結果。 */
    Error Display_fill_circle(uint32_t receiver, int32_t x, int32_t y, int32_t radius, int32_t color) override;
    /** @brief バッファをI2Cへ分割転送する。@param receiver ハンドル。@return 実行結果またはNativePending。 */
    Error Display_display(uint32_t receiver) override;
    /** @brief シリアルへ改行付きで出力する。@param text 長さ付き文字列。@return 実行結果またはNativePending。 */
    Error function_print(device_api::StringView text) override;

    private:
    /** @brief バックエンド内の機器種別。JSONのクラスIDとは独立。 */
    enum class Kind : uint8_t {
        Servo,
        IMU,
        Distance,
        Display
    };
    /** @brief 1実行で保持するハンドル情報。 */
    struct Object {
        uint32_t handle = 0;
        Kind kind = Kind::Servo;
        uint8_t channel = 0;
        uint8_t mode = 0;
        bool ownsOutput = false;
    };
    /** @brief ハンドルと種別を検査する。@param handle 対象。@param kind 必要種別。@return 有効な情報またはnull。 */
    Object *find(uint32_t handle, Kind kind);
    /** @brief 新しいハンドルを確保する。@param kind 種別。@param result 出力。@return 成否。 */
    Error create(Kind kind, uint32_t &result);
    /** @brief 表示機器・色を検査する。@param receiver ハンドル。@param color 色。@return 有効ならtrue。 */
    bool drawable(uint32_t receiver, int32_t color);
    /** @brief 描画時間とint16演算の範囲を検査する。@param value 座標。@return 有効ならtrue。 */
    static bool coordinate(int32_t value);
    TwoWire &wire_;
    uroa::Pca9685Servos &servos_;
    uroa::Mpu6050 &imu_;
    DistanceCallbacks distance_;
    Adafruit_SSD1306 display_;
    Object objects_[32];
    uint32_t nextHandle_ = 1;
    bool displayReady_ = false;
    bool flushing_ = false;
    size_t displayOffset_ = 0;
    size_t textOffset_ = 0;
    size_t serialOffset_ = 0;
};
