/**
 * @file main.cpp
 * @brief diagramのBLE/Wi-Fi指令でサーボを操作し、登録されたIMU・距離の測定値を返す。
 * @details 通信タスクは指令をキューへコピーし、I2C・応答・停止・送信はloop()で行う。
 * HC-SR04のGPIOと同期測定は専用タスクだけが操作し、測定結果をキューでloop()へ渡す。
 */
#include <Arduino.h>
#include <Wire.h>
#include <uroa_ble.h>
#include <uroa_imu.h>
#include <uroa_protocol.h>
#include <uroa_servo.h>
#include <uroa_ultrasonic.h>
#include <atomic>
#include <cstring>
#include <math.h>
#include "config.h"
#include "network.h"
#include "bytecode_receiver.h"
#include "bytecode_vm.h"
#include "device_native.h"

namespace {
    using uroa::Pca9685Servos;
    using uroa::Protocol;

    constexpr size_t DeviceInfoMaxLength = uroa::BleTransport::DeviceInfoMaxLength; ///< 機器情報の最大バイト数。
    constexpr size_t DeviceTelemetryMaxLength = Protocol::DeviceTelemetryHeaderLength +
        DeviceInfoMaxLength * Protocol::IMU6AxisRecordLength; ///< 最大20個のIMUレコードとヘッダー。
    constexpr uint16_t NotificationHeaderLength =
        uroa::BleTransport::NotificationHeaderLength; ///< ATT通知のヘッダー長。

    /** @brief diagramのDeviceTypeと共通の機器種別。宣言の受付はドライバーの実装・機器の存在を示さない。 */
    enum class DeviceType : uint8_t {
        IMU6Axis = 0,
        IMU9Axis = 1,
        UltrasonicDistanceSensor = 2,
        Camera = 3,
        ServoMotor = 128,
    };

    /** @brief 受信済み機器情報。channelとmodeはサーボの場合だけ使う。 */
    struct DeviceInfoEntry {
        DeviceType type;
        uint8_t channel = 255;
        uint8_t mode = 0;
    };

    DeviceInfoEntry deviceInfos[DeviceInfoMaxLength]; ///< loop()だけが更新する、受信順の機器一覧。
    size_t deviceInfoCount = 0; ///< 一覧の有効件数。Wi-Fi切り替え時は維持し、終了・飽和時にloop()で消去する。
    bool deviceInfoReceived = false; ///< STREAM開始前の機器登録状態。loop()だけが更新する。

    uroa::BleTransport bluetooth; ///< 稼働中保持するBLEサーバー。
    BytecodeReceiver bytecodeReceiver; ///< loop()で分割受信を検証し、確定後にVMへ渡す。
    BytecodeVM bytecodeVm; ///< loop()だけが操作するVM。受信器の確定バッファを借用する。
    uint32_t vmRevision = 0; ///< VMが照合した確定プログラムの世代。更新だけでは開始しない。
    BytecodeVM::State reportedVmState = BytecodeVM::State::Idle; ///< シリアルへ報告済みの状態。

    uroa::Mpu6050 imu(Wire); ///< loop()からだけI2C操作するIMU。

    /**
     * @brief config.hのGPIO割り当てを超音波センサーの設定へ渡す。
     * @return 教材側が指定したTRIGとECHOのGPIO設定。
     */
    uroa::HcSr04::Config ultrasonicConfig() {
        uroa::HcSr04::Config config;
        config.triggerPin = ULTRASONIC_TRIGGER_PIN;
        config.echoPin = ULTRASONIC_ECHO_PIN;
        return config;
    }

    uroa::HcSr04 ultrasonic(ultrasonicConfig()); ///< 初期化・測定・公開値の参照はultrasonicTask()だけが行う。

    /** @brief 超音波測定タスクからloop()へ渡す、測定要求の世代と距離。 */
    struct UltrasonicSample {
        uint32_t request = 0;
        uint32_t measuredAt = 0; ///< 測定を終えたmillis。VMは古い結果を数値として返さない。
        uint16_t distanceMm = Protocol::InvalidDistanceMm;
    };

    QueueHandle_t ultrasonicSamples = nullptr; ///< setup()が確保する最新1件の測定結果キュー。稼働中は破棄しない。
    std::atomic<bool> ultrasonicReady{false}; ///< 超音波タスクが更新するGPIO設定の結果。センサー検出ではない。
    std::atomic<uint32_t> ultrasonicRequest{0}; ///< loop()が更新する要求。bit0は測定有効、残りは要求の世代。

    /**
     * @brief 教材の設定から角度サーボの設定を組み立てる。
     * @return ピンを含まないPCA9685とサーボの設定。
     */
    Pca9685Servos::Config servoConfig() {
        Pca9685Servos::Config config;
        config.address = PCA_ADDRESS;
        config.frequency = SERVO_FREQUENCY;
        config.minAngle = MIN_ANGLE;
        config.maxAngle = MAX_ANGLE;
        config.minPulseUs = MIN_PULSE_US;
        config.maxPulseUs = MAX_PULSE_US;
        return config;
    }

    Pca9685Servos servos(Wire, servoConfig()); ///< 明示的なARMまで出力しない角度サーボ。

    /** @brief 受信指令と、その受信元・接続世代を保持する。 */
    struct Queued {
        Protocol::Command command;
        uint32_t session;
        bool wifi = false;
        bool isDeviceInfo = false;
        uint8_t deviceInfoLength = 0;
        uint8_t deviceInfoData[DeviceInfoMaxLength]{};
    };

    QueueHandle_t commands = nullptr; ///< 通信タスクからloop()へ渡す16件の指令キュー。
    std::atomic<bool> mustStop{false}; ///< 接続変更・キュー飽和時の出力とSTREAMの停止要求。
    std::atomic<bool> resetDeviceInfo{false}; ///< 接続終了・新規BLE接続・キュー飽和時の機器一覧の消去要求。
    std::atomic<bool> urgentStop{false}; ///< 通常の指令キューを迂回するSTOP要求。
    std::atomic<uint16_t> urgentSeq{0}; ///< 優先STOPに応答するsequence。
    uint16_t streamHz = 0; ///< 登録センサーの通知周期。0は停止、5/10/20Hzを受け付ける。
    uint16_t sampleSeq = 0; ///< センサー通知sequence。16bitで周回する。
    uint32_t lastCommand = 0; ///< 最後に有効形式の指令を受け付けた時刻（ms）。
    uint32_t lastSample = 0; ///< 最後にセンサー通知を組み立てた時刻（ms）。
    bool vmDistanceActive = false; ///< VMの距離要求。STREAMの要求とは独立し、loop()だけが更新する。
    uint32_t vmDistanceStarted = 0; ///< 初回測定の待機期限を判定するmillis。

    /** @brief 専用タスクのGPIO初期化結果を読む。@return 測定要求を受け取れるならtrue。 */
    bool nativeDistanceReady();
    /** @brief VMの距離要求だけを変更する。@param enabled 有効にするか。 */
    void enableNativeDistance(bool enabled);
    /** @brief 最新距離をVMへ渡す。@param result mm。@return 成功・待機・失敗。 */
    BytecodeVM::Error readNativeDistance(float &result);

    DeviceNative nativeDevices(Wire, servos, imu, {nativeDistanceReady, enableNativeDistance, readNativeDistance});
    BytecodeVM::NativeApi nativeApi = device_api::makeNativeApi(nativeDevices); ///< setup()でVMへ登録する静的な表。

    /**
     * @brief 確定バッファの置換で旧VMを停止し、開始済みのVMを最大128命令ずつ実行する。
     * @param stopped このloop()でSTOP・接続変更・通信期限切れを処理した場合true。
     * @details 受信器のバッファ置換後は、旧バッファを使うtick()より前にstart()またはstop()する。
     *
     * 正常COMMITは保持だけを行い、自動実行しない。VM_START=10で保持コードを先頭から開始し、
     * VM_STOP=11でVMと所有機器を停止する。Command v1のCH=255、value=duration=0を必須とする。
     * Protocolの操作番号とWeb側のpython/python-block.tsが共通契約を持ち、ble-wifi/ble.tsが
     * 最後の転送・現在の接続・取消を確認する。開始ACKはstart()の結果までで、終了通知ではない。
     * 1周最大128命令を実行し、通常の指令・サーボ・通信・停止処理へ制御を返す。
     * STOP、切断、Wi-Fi切替、接続変更、通信キュー飽和、5秒の通信期限切れでもVMを停止する。
     * 再接続やPINGで自動再開しない。保持コードはVM_STARTで再実行できる。
     * Pythonのsleep待機でもloop()を継続し、各周回の停止・通信処理を優先する。待機の仕様は
     * device_native.cppのfunction_sleepとbytecode_vm.cppのcallNativeを参照する。
     *
     * 受信ACKはCRC・形式検証・RAM保持の完了を表し、VMの開始・正常終了を保証しない。開始時のメモリ確保失敗や実行エラーはシリアルで確認する。
     *
     * ## 所有権と確認範囲
     *
     * VMは受信器の確定バッファを借用し、コピーしない。受信器の置換後は、loop()がVMを進める前に世代を照合し、新しいstart()かstop()を呼ぶ。BLEコールバックからVMを操作しない。未完了・不正な転送は確定済みバッファを置き換えず、以前のVMの実行を直接開始し直さない。
     *
     * 通常指令と同様、loop()は停止要求を先に処理してから受信処理を進める。
     *
     * STOPはサーボとVMの実行を停止する。確定データは消去しない。
     *
     * VMは1周128命令まで進め、展開した構造体とネイティブ呼出しに対応する。VMの規約は
     * bytecode_vm.cppの各処理を参照。device_api.generated.cppのID表でDeviceNativeを呼び出す。
     * HALT後は最後のサーボ指令を保持し、STOP・切断・再送で解放する。エラーではVMの出力を停止する。
     */
    void runBytecode(bool stopped) {
        const bool connected = bluetooth.connected.load() && !network.connected.load() && !network.bleUnavailable();
        const bool cancelled = stopped || mustStop.load() || urgentStop.load() || !connected;
        if (vmRevision != bytecodeReceiver.programRevision) {
            vmRevision = bytecodeReceiver.programRevision;
            bytecodeVm.stop();
            reportedVmState = BytecodeVM::State::Idle;
        }
        if (cancelled) {
            bytecodeVm.stop();
        }
        bytecodeVm.tick(128);
        if (reportedVmState == bytecodeVm.state) {
            return;
        }
        reportedVmState = bytecodeVm.state;
        Serial.printf("VM state=%u instructions=%lu\n", unsigned(bytecodeVm.state),
            static_cast<unsigned long>(bytecodeVm.instructions));
        /**
         * @details
         * エラー時は`VM error=番号 pc=位置`を出す。
         */
        if (bytecodeVm.state == BytecodeVM::State::Error) {
            nativeDevices.reset();
            Serial.printf("VM error=%u pc=%lu\n", unsigned(bytecodeVm.error),
                static_cast<unsigned long>(bytecodeVm.errorPc));
        }
        /**
         * @details
         * HALT時には先頭16個までを`VM g[番号]=値`としてシリアルへ出す。
         * 番号は構造体の葉を展開した大域スロット。ObjectはクラスIDと不透明ハンドルを表示する。
         */
        if (bytecodeVm.state == BytecodeVM::State::Halted) {
            const uint32_t count = bytecodeVm.globalCount < 16 ? bytecodeVm.globalCount : 16;
            for (uint32_t index = 0; index < count; ++index) {
                const BytecodeVM::Value &value = bytecodeVm.globals[index];
                Serial.printf("VM g[%lu]=", static_cast<unsigned long>(index));
                if (value.type == BytecodeVM::Type::Integer) {
                    Serial.println(value.integer);
                }
                else if (value.type == BytecodeVM::Type::Float) {
                    Serial.printf("%.9g\n", double(value.real));
                }
                else if (value.type == BytecodeVM::Type::Object) {
                    Serial.printf("Object(class=%u, handle=%lu)\n", unsigned(value.classId),
                        static_cast<unsigned long>(value.address));
                }
                else if (value.type == BytecodeVM::Type::String) {
                    const char *bytes = nullptr;
                    size_t length = 0;
                    if (bytecodeVm.stringBytes(value, bytes, length)) {
                        Serial.write(reinterpret_cast<const uint8_t *>(bytes), length);
                        Serial.println();
                    }
                    else {
                        Serial.println("invalid string");
                    }
                }
                else {
                    Serial.println(value.type == BytecodeVM::Type::None ? "None" : "uninitialized");
                }
            }
        }
    }

    /**
     * @brief 測定要求の世代を進め、過去の超音波サンプルを無効化する。
     * @param enabled 登録済み距離センサーのSTREAMが有効ならtrue。
     * @details loop()だけが呼ぶ。測定途中の旧世代の結果は送信側で破棄する。
     */
    void setUltrasonicSampling(bool enabled) {
        const uint32_t next = (ultrasonicRequest.load() + 2U) & ~uint32_t(1);
        // 物理センサーはSTREAMとVMで共有し、どちらか一方の要求で測定する。
        ultrasonicRequest.store(next | (enabled || vmDistanceActive ? 1U : 0U));
    }

    /** @brief loop()から通知と超音波測定要求を停止し、旧サンプルを無効化する。 */
    void stopTelemetry() {
        streamHz = 0;
        setUltrasonicSampling(false);
    }

    /**
     * @brief HC-SR04を初期化し、有効な要求について100ms以上あけて同期測定する。
     * @param unused FreeRTOSのタスク引数。このタスクでは使用しない。
     * @details GPIOとHcSr04の状態はこのタスクだけで扱い、loop()のSTOP・サーボ処理を待たせない。
     */
    void ultrasonicTask(void *unused) {
        (void)unused;
        const bool initialized = ultrasonic.begin();
        Serial.printf("HC-SR04 config: TRIG=GPIO%d ECHO=GPIO%d GPIO init=%s\n", ULTRASONIC_TRIGGER_PIN,
            ULTRASONIC_ECHO_PIN, initialized ? "OK" : "FAILED");
        ultrasonicReady.store(initialized);
        if (!initialized) {
            vTaskDelete(nullptr);
            return;
        }
        while (true) {
            const uint32_t request = ultrasonicRequest.load();
            if ((request & 1U) != 0) {
                const uroa::HcSr04::Result result = ultrasonic.read();
                if (result != uroa::HcSr04::Result::TooSoon) {
                    UltrasonicSample sample;
                    sample.request = request;
                    sample.measuredAt = millis();
                    if (result == uroa::HcSr04::Result::Ok) {
                        sample.distanceMm = static_cast<uint16_t>(lroundf(ultrasonic.distanceCm * 10.0f));
                    }
                    if (ultrasonicRequest.load() == request) {
                        xQueueOverwrite(ultrasonicSamples, &sample);
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(ULTRASONIC_SAMPLE_INTERVAL_MS));
        }
    }

    /**
     * @brief 現在の測定要求と世代が一致する最新の距離だけをキューから読む。
     * @return 距離mm。未測定・失敗・旧世代・測定停止中ではInvalidDistanceMm。
     * @details loop()からだけ呼ぶ。20Hz送信では同じ測定値を複数回使うことがある。
     */
    uint16_t currentDistanceMm() {
        const uint32_t request = ultrasonicRequest.load();
        UltrasonicSample sample;
        if ((request & 1U) == 0 || ultrasonicSamples == nullptr ||
            xQueuePeek(ultrasonicSamples, &sample, 0) != pdTRUE || sample.request != request) {
            return Protocol::InvalidDistanceMm;
        }
        return sample.distanceMm;
    }

    /**
     * @brief StatusとTelemetryへ含める機器・制御状態を組み立てる。
     * @return bit0=PCA正常、bit1=IMU正常、bit2=CH有効、bit3=STREAM有効。
     */
    uint8_t stateFlags() {
        return (servos.available ? 1 : 0) | (imu.available ? 2 : 0) | (servos.anyArmed() ? 4 : 0) | (streamHz ? 8 : 0);
    }

    /**
     * @brief 受付結果と内部指令値をBLE/Wi-Fiへ通知する。
     * @param code Protocolの受付結果コード。
     * @param sequence ACK対象の番号。0は自発通知。
     * @param channel CH0〜15、全体通知は255。
     */
    void status(uint8_t code, uint16_t sequence, uint8_t channel = 255) {
        uint8_t packet[12];
        const auto *state = channel < 16 ? &servos.states[channel] : nullptr;
        const bool speed = state && state->mode == Pca9685Servos::Mode::Speed;
        const uint16_t current = speed ? uint16_t(int16_t(lroundf(state->speed * 100))) : state ? state->current : 0;
        const uint16_t target = speed ? current : state ? state->target : 0;
        Protocol::status(packet, code, sequence, stateFlags() | (speed ? Protocol::FLAG_SPEED : 0), channel, current,
            target, servos.remaining(channel, millis()));
        if (!network.bleUnavailable()) {
            bluetooth.publishStatus(packet, sizeof packet);
        }
        network.publish(packet, sizeof packet);
    }

    /**
     * @brief 通信タスクから指令をコピーし、STOPを優先フラグへ渡す。
     * @param data コールバック中だけ有効な受信データ。
     * @param length 受信バイト数。
     * @param generation 受信元の接続世代。
     * @param wifi Wi-Fi経由ならtrue、BLEならfalse。
     * @details I2Cを操作しない。Wi-Fi制御権がある間はBLE指令を受け付けない。
     */
    void receive(const uint8_t *data, size_t length, uint32_t generation, bool wifi) {
        if (!wifi && network.connected.load()) {
            return;
        }
        if (wifi && length != 10) {
            return;
        }
        if (Protocol::urgentStop(data, length)) {
            urgentSeq.store(Protocol::read16(data + 2));
            urgentStop.store(true);
            return;
        }
        Queued queued{};
        queued.command = Protocol::command(data, length);
        queued.session = generation;
        queued.wifi = wifi;
        if (xQueueSend(commands, &queued, 0) != pdTRUE) {
            resetDeviceInfo.store(true);
            mustStop.store(true);
            if (wifi) {
                network.stopControl();
            }
        }
    }

    /**
     * @brief BLEタスクの機器情報を現在の接続世代とともにキューへコピーする。
     * @param data コールバック中だけ有効な受信データ。
     * @param length 受信バイト数。0は機器一覧の消去。
     * @param generation 受信元のBLE接続世代。
     * @details I2Cと機器情報の解釈は行わない。キュー飽和時は既存指令と同じ停止要求を出す。
     */
    void receiveDeviceInfo(const uint8_t *data, size_t length, uint32_t generation) {
        if (network.connected.load() || !bluetooth.connected.load() || length > DeviceInfoMaxLength) {
            return;
        }
        Queued queued{};
        queued.session = generation;
        queued.isDeviceInfo = true;
        queued.deviceInfoLength = static_cast<uint8_t>(length);
        if (length > 0) {
            std::memcpy(queued.deviceInfoData, data, length);
        }
        if (xQueueSend(commands, &queued, 0) != pdTRUE) {
            resetDeviceInfo.store(true);
            mustStop.store(true);
        }
    }

    /**
     * @brief 機器情報を全件検証し、成功時だけ受信順の一覧を置き換える。
     * @param data 種別1バイト、サーボの場合は続けてCH・モードを持つ受信データ。
     * @param length 受信バイト数。0から20。
     * @details loop()から呼ぶ。登録のみで機器初期化・サーボのARM・計測開始は行わない。
     */
    void handleDeviceInfo(const uint8_t *data, size_t length) {
        if (length > DeviceInfoMaxLength) {
            Serial.println("DeviceInfo rejected: payload too long");
            return;
        }
        DeviceInfoEntry received[DeviceInfoMaxLength];
        size_t count = 0;
        size_t offset = 0;
        while (offset < length) {
            DeviceInfoEntry entry{};
            entry.type = static_cast<DeviceType>(data[offset]);
            switch (entry.type) {
                case DeviceType::IMU6Axis:
                case DeviceType::IMU9Axis:
                case DeviceType::UltrasonicDistanceSensor:
                case DeviceType::Camera:
                    ++offset;
                    break;
                case DeviceType::ServoMotor:
                    if (length - offset < 3 || data[offset + 1] >= 16 || data[offset + 2] > 1) {
                        Serial.println("DeviceInfo rejected: invalid servo record");
                        return;
                    }
                    entry.channel = data[offset + 1];
                    entry.mode = data[offset + 2];
                    offset += 3;
                    break;
                default:
                    Serial.println("DeviceInfo rejected: unknown device type");
                    return;
            }
            received[count] = entry;
            ++count;
        }
        if (count > 0) {
            std::memcpy(deviceInfos, received, count * sizeof(DeviceInfoEntry));
        }
        deviceInfoCount = count;
        deviceInfoReceived = true;
        // 一覧の置換では旧測定を持ち越さず、新しいSTREAM指令を待つ。
        stopTelemetry();
        Serial.printf("DeviceInfo accepted: bytes=%u devices=%u\n", static_cast<unsigned>(length),
            static_cast<unsigned>(deviceInfoCount));
        for (size_t index = 0; index < deviceInfoCount; ++index) {
            const auto &entry = deviceInfos[index];
            Serial.printf("DeviceInfo[%u]: type=%u channel=%u mode=%u\n", static_cast<unsigned>(index),
                static_cast<unsigned>(entry.type), static_cast<unsigned>(entry.channel),
                static_cast<unsigned>(entry.mode));
        }
    }

    /**
     * @brief 指定種別のセンサーが現在のDeviceInfoにあるか調べる。
     * @param type 探すセンサー種別。
     * @return 1件でも登録されていればtrue。
     */
    bool hasSensor(DeviceType type) {
        for (size_t index = 0; index < deviceInfoCount; ++index) {
            if (deviceInfos[index].type == type) {
                return true;
            }
        }
        return false;
    }

    /** @brief VMが使う測定タスクの準備状態を確認する。@return 準備済みならtrue。 */
    bool nativeDistanceReady() {
        return ultrasonicReady.load() && ultrasonicSamples != nullptr;
    }

    /** @brief VMの距離要求を切り替え、STREAMと合成する。@param enabled 有効にするか。
     * @details 同じ値の再設定は世代を変えず、複数の距離オブジェクトで最新1件を共有する。
     */
    void enableNativeDistance(bool enabled) {
        if (vmDistanceActive == enabled) {
            return;
        }
        vmDistanceActive = enabled;
        vmDistanceStarted = millis();
        setUltrasonicSampling(streamHz != 0 && hasSensor(DeviceType::UltrasonicDistanceSensor));
    }

    /** @brief 最新の測定値をmmで返す。@param result mm。@return 成功・NativePending・NativeFailure。
     * @details 初回待ちは最大1秒。未測定・失敗の0xFFFFと1秒を超えた古い値を有効な距離へ変換しない。
     */
    BytecodeVM::Error readNativeDistance(float &result) {
        using Error = BytecodeVM::Error;
        if (!vmDistanceActive || !nativeDistanceReady()) {
            return Error::NativeFailure;
        }
        UltrasonicSample sample;
        if (xQueuePeek(ultrasonicSamples, &sample, 0) != pdTRUE || sample.request != ultrasonicRequest.load()) {
            return millis() - vmDistanceStarted < 1000 ? Error::NativePending : Error::NativeFailure;
        }
        if (sample.distanceMm == Protocol::InvalidDistanceMm || millis() - sample.measuredAt > 1000) {
            return Error::NativeFailure;
        }
        result = float(sample.distanceMm);
        return Error::None;
    }

    /**
     * @brief DeviceInfoからサーボを除いたセンサー種別を受信順に取り出す。
     * @param sensorTypes 最大20件の種別を受け取る配列。
     * @return 配列へ格納したセンサー件数。未対応種別も長さ検査用に残す。
     */
    size_t copySensorTypes(uint8_t (&sensorTypes)[DeviceInfoMaxLength]) {
        size_t count = 0;
        for (size_t index = 0; index < deviceInfoCount; ++index) {
            if (deviceInfos[index].type != DeviceType::ServoMotor) {
                sensorTypes[count] = static_cast<uint8_t>(deviceInfos[index].type);
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief 登録センサーの可変長通知に必要な容量を求める。
     * @return 通知長。未対応のセンサー種別を含む場合は0。
     */
    size_t deviceTelemetryLength() {
        uint8_t sensorTypes[DeviceInfoMaxLength];
        const size_t count = copySensorTypes(sensorTypes);
        return Protocol::deviceTelemetryLength(sensorTypes, count);
    }

    /**
     * @brief 登録された機器だけを読み、同じTelemetryをBLEとWi-Fiへ渡す。
     * @param now 通知を組み立てる時刻millis。距離は専用タスクの最新結果を使う。
     * @details loop()からだけ呼ぶ。IMUを含まなければI2C読み取りは行わない。
     */
    void publishDeviceTelemetry(uint32_t now) {
        uint8_t sensorTypes[DeviceInfoMaxLength];
        const size_t count = copySensorTypes(sensorTypes);
        const size_t length = Protocol::deviceTelemetryLength(sensorTypes, count);
        if (length == 0 || (!network.connected.load() && length + NotificationHeaderLength > bluetooth.mtu.load())) {
            stopTelemetry();
            status(length == 0 ? Protocol::CODE_HARDWARE : Protocol::CODE_RANGE, 0);
            return;
        }
        uint8_t raw[14];
        const bool needsIMU = hasSensor(DeviceType::IMU6Axis);
        if (needsIMU && (!imu.available || !imu.readRaw(raw))) {
            stopTelemetry();
            status(Protocol::CODE_IO_ERROR, 0);
            return;
        }
        if (mustStop.load() || urgentStop.load()) {
            return;
        }
        uint8_t packet[DeviceTelemetryMaxLength];
        const size_t written = Protocol::deviceTelemetry(packet, sizeof packet, stateFlags(), ++sampleSeq, now,
            sensorTypes, count, needsIMU ? raw : nullptr, currentDistanceMm());
        if (written != length) {
            stopTelemetry();
            status(Protocol::CODE_FORMAT, 0);
            return;
        }
        if (!network.bleUnavailable()) {
            bluetooth.publishTelemetry(packet, written);
        }
        network.publish(packet, written);
    }

    /**
     * @brief サーボ操作の結果を通信の応答コードへ変換する。
     * @param result 共通サーボライブラリーの結果。
     * @return Protocolの受付結果コード。
     */
    uint8_t resultCode(Pca9685Servos::Result result) {
        switch (result) {
            case Pca9685Servos::Result::Ok:
                return Protocol::CODE_OK;
            case Pca9685Servos::Result::Range:
                return Protocol::CODE_RANGE;
            case Pca9685Servos::Result::Unavailable:
                return Protocol::CODE_HARDWARE;
            case Pca9685Servos::Result::NotArmed:
                return Protocol::CODE_NOT_ARMED;
            case Pca9685Servos::Result::WrongMode:
                return Protocol::CODE_MODE;
            default:
                return Protocol::CODE_IO_ERROR;
        }
    }

    /**
     * @brief loop()内で指令を検査し、機器操作と受付結果の通知を行う。
     * @param command 現在の接続世代で受信した指令。
     * @param allowVmStart このloop()で停止要求がなければtrue。停止した周回ではVM開始を拒否する。
     */
    void handle(const Protocol::Command &command, bool allowVmStart) {
        const auto *bytes = command.bytes;
        const uint16_t sequence = Protocol::read16(bytes + 2);
        const uint16_t value = Protocol::read16(bytes + 6);
        const uint16_t duration = Protocol::read16(bytes + 8);
        const uint8_t operation = bytes[1];
        const uint8_t channel = bytes[4];
        if (!Protocol::valid(command)) {
            status(Protocol::CODE_FORMAT, sequence);
            return;
        }
        lastCommand = millis();
        // VMはloop()だけが操作する。確定バッファの置換後も旧イメージを読む前にstop()する。
        if (operation == Protocol::VM_START || operation == Protocol::VM_STOP) {
            if (channel != 255 || value != 0 || duration != 0) {
                status(Protocol::CODE_RANGE, sequence);
                return;
            }
            if (!bluetooth.connected.load() || network.connected.load() || network.bleUnavailable()) {
                status(Protocol::CODE_NOT_ARMED, sequence);
                return;
            }
            if (operation == Protocol::VM_START &&
                (!allowVmStart || mustStop.load() || urgentStop.load())) {
                status(Protocol::CODE_BUSY, sequence);
                return;
            }
            bytecodeVm.stop();
            vmRevision = bytecodeReceiver.programRevision;
            reportedVmState = BytecodeVM::State::Stopped;
            if (operation == Protocol::VM_STOP) {
                status(Protocol::CODE_OK, sequence);
                return;
            }
            if (!bytecodeReceiver.program || bytecodeReceiver.programLength == 0) {
                status(Protocol::CODE_NOT_ARMED, sequence);
                return;
            }
            const bool started = bytecodeVm.start(bytecodeReceiver.program.get(), bytecodeReceiver.programLength);
            status(started ? Protocol::CODE_OK : Protocol::CODE_FORMAT, sequence);
            return;
        }
        if (operation == Protocol::PING || operation == Protocol::INFO) {
            status(Protocol::CODE_OK, sequence, channel);
            return;
        }
        if (operation == Protocol::STREAM) {
            if (value != 0 && value != 5 && value != 10 && value != 20) {
                status(Protocol::CODE_RANGE, sequence);
                return;
            }
            if (value) {
                if (!deviceInfoReceived) {
                    Serial.println("DeviceTelemetry requires DeviceInfo before STREAM");
                    status(Protocol::CODE_FORMAT, sequence);
                    return;
                }
                const size_t length = deviceTelemetryLength();
                if (length == 0) {
                    Serial.println("DeviceTelemetry unavailable: unsupported sensor type");
                    status(Protocol::CODE_HARDWARE, sequence);
                    return;
                }
                if ((hasSensor(DeviceType::IMU6Axis) && !imu.available) ||
                    (hasSensor(DeviceType::UltrasonicDistanceSensor) && !ultrasonicReady.load())) {
                    status(Protocol::CODE_HARDWARE, sequence);
                    return;
                }
                if (!network.connected.load() && length + NotificationHeaderLength > bluetooth.mtu.load()) {
                    Serial.printf("DeviceTelemetry requires ATT MTU >= %u; current=%u\n",
                        static_cast<unsigned>(length + NotificationHeaderLength),
                        static_cast<unsigned>(bluetooth.mtu.load()));
                    status(Protocol::CODE_RANGE, sequence);
                    return;
                }
            }
            streamHz = value;
            setUltrasonicSampling(value != 0 && hasSensor(DeviceType::UltrasonicDistanceSensor));
            lastSample = millis();
            status(Protocol::CODE_OK, sequence);
            return;
        }
        if (operation == Protocol::STOP) {
            servos.allOff();
            status(Protocol::CODE_OK, sequence);
            return;
        }
        if (channel >= 16) {
            status(Protocol::CODE_RANGE, sequence);
            return;
        }
        Pca9685Servos::Result result;
        if (operation == Protocol::DISABLE) {
            result = servos.disable(channel);
        }
        else if (operation == Protocol::ARM_SPEED) {
            result = value == 0 && duration == 0 ? servos.armSpeed(channel) : Pca9685Servos::Result::Range;
        }
        else if (operation == Protocol::SET_SPEED) {
            result = duration == 0 ? servos.setSpeed(channel, Protocol::readSigned16(bytes + 6) / 100.0f)
                                   : Pca9685Servos::Result::Range;
        }
        else if (operation == Protocol::ARM) {
            result = duration <= 10000 ? servos.armAngle(channel, value / 100.0f) : Pca9685Servos::Result::Range;
        }
        else {
            result = servos.moveAngle(channel, value / 100.0f, duration, millis());
        }
        // 出力失敗の自発通知と指令への受付結果を別々に返す。
        if (result == Pca9685Servos::Result::IoError && operation != Protocol::DISABLE) {
            status(Protocol::CODE_IO_ERROR, 0, channel);
        }
        status(resultCode(result), sequence, channel);
    }
}

/**
 * @brief I2C機器、超音波測定タスク、指令キュー、Wi-FiとBLEを開始する。
 * @details 起動時にサーボを自動ARMしない。接続変更時も停止を予約する。
 */
void setup() {
    Serial.begin(115200);
    delay(200);
    const bool i2cStarted = Wire.begin(SDA_PIN, SCL_PIN);
    nativeDevices.i2cReady = i2cStarted;
    Wire.setClock(100000);
    Wire.setTimeOut(30);
    Serial.println("ESP32 diagram-control: servo outputs OFF at startup");
    Serial.printf(
        "I2C config: SDA=GPIO%d SCL=GPIO%d clock=100000 Hz init=%s\n", SDA_PIN, SCL_PIN, i2cStarted ? "OK" : "FAILED");
    if (i2cStarted) {
        Serial.print("I2C:");
        for (uint8_t address = 1; address < 127; ++address) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0) {
                Serial.printf(" 0x%02X", address);
            }
        }
        Serial.println();
        servos.begin();
        imu.begin();
    }
    Serial.printf("PCA9685=%s MPU6050=%s (0x%02X)\n", servos.available ? "OK" : "MISSING",
        imu.available ? "OK" : "MISSING", imu.address);
    if (!bytecodeVm.bindNativeApi(&nativeApi)) {
        Serial.println("VM native API registration failed");
    }
    commands = xQueueCreate(16, sizeof(Queued));
    if (!commands) {
        servos.allOff();
        Serial.println("Queue allocation failed");
        while (true) {
            delay(1000);
        }
    }

    if (ULTRASONIC_TRIGGER_PIN >= 0 && ULTRASONIC_ECHO_PIN >= 0) {
        ultrasonicSamples = xQueueCreate(1, sizeof(UltrasonicSample));
        if (ultrasonicSamples == nullptr) {
            Serial.println("HC-SR04 sample queue allocation failed");
        }
        else if (xTaskCreate(ultrasonicTask, "hc-sr04", 3072, nullptr, 1, nullptr) != pdPASS) {
            vQueueDelete(ultrasonicSamples);
            ultrasonicSamples = nullptr;
            Serial.println("HC-SR04 task allocation failed");
        }
    }

    uroa::WifiNetwork::Callbacks wifiCallbacks;
    wifiCallbacks.command = [](const uint8_t *data, size_t length, uint32_t generation) {
        receive(data, length, generation, true);
    };
    wifiCallbacks.session = [](bool connected, uint32_t) {
        // BLEで受信した一覧をWi-Fi制御権の取得時だけ引き継ぐ。
        if (!connected) {
            resetDeviceInfo.store(true);
        }
        mustStop.store(true);
    };
    wifiCallbacks.startBLE = [] {
        bluetooth.start();
    };
    wifiCallbacks.stopBLE = [] {
        bluetooth.stop();
    };
    beginNetwork(wifiCallbacks);

    uroa::BleTransport::Config bleConfig;
    bleConfig.name = DEVICE_NAME;
    bleConfig.serviceUuid = SERVICE_UUID;
    bleConfig.commandUuid = COMMAND_UUID;
    bleConfig.telemetryUuid = TELEMETRY_UUID;
    bleConfig.statusUuid = STATUS_UUID;
    bleConfig.configUuid = CONFIG_UUID;
    bleConfig.resultUuid = RESULT_UUID;
    bleConfig.deviceInfoUuid = DEVICE_INFO_UUID;
    bleConfig.bytecodeUuid = BYTECODE_UUID;
    bleConfig.bytecodeResultUuid = BYTECODE_RESULT_UUID;
    bleConfig.telemetryMaxLength = DeviceTelemetryMaxLength;
    bleConfig.preferredMtu = DeviceTelemetryMaxLength + NotificationHeaderLength;
    bleConfig.passkey = PAIRING_CODE;
    bytecodeReceiver.begin([](const uint8_t *data, size_t length) {
        bluetooth.publishBytecodeResult(data, length);
    }, [](uint32_t generation) {
        return bluetooth.connected.load() && bluetooth.generation.load() == generation && !network.connected.load() &&
            !network.bleUnavailable();
    });
    uroa::BleTransport::Callbacks bleCallbacks;
    bleCallbacks.command = [](const uint8_t *data, size_t length, uint32_t generation) {
        receive(data, length, generation, false);
    };
    bleCallbacks.deviceInfo = [](const uint8_t *data, size_t length, uint32_t generation) {
        receiveDeviceInfo(data, length, generation);
    };
    bleCallbacks.bytecode = [](const uint8_t *data, size_t length, uint32_t generation) {
        bytecodeReceiver.enqueue(data, length, generation);
    };
    bleCallbacks.session = [](bool, uint32_t) {
        if (!network.connected.load()) {
            resetDeviceInfo.store(true);
        }
        mustStop.store(true);
    };
    bleCallbacks.configure = [](const String &input) {
        return network.configure(input);
    };
    bleCallbacks.started = [] {
        status(Protocol::CODE_OK, 0);
        bytecodeReceiver.publishResult();
    };
    bluetooth.begin(bleConfig, bleCallbacks);
    Serial.printf("BLE address: %s\n", NimBLEDevice::getAddress().toString().c_str());
}

/** @brief 停止を優先し、現接続の指令、サーボ補間、期限監視と登録センサーの通知を進める。 */
void loop() {
    uint32_t now = millis();
    bool vmStopRequested = false;
    if (mustStop.exchange(false)) {
        vmStopRequested = true;
        servos.allOff();
        stopTelemetry();
        if (resetDeviceInfo.exchange(false)) {
            deviceInfoCount = 0;
            deviceInfoReceived = false;
        }
        lastCommand = now;
        xQueueReset(commands);
        status(Protocol::CODE_OK, 0);
    }
    if (urgentStop.exchange(false)) {
        vmStopRequested = true;
        servos.allOff();
        xQueueReset(commands);
        lastCommand = now;
        status(Protocol::CODE_OK, urgentSeq.load());
    }
    bytecodeReceiver.process(bluetooth.connected.load() && !network.connected.load() && !network.bleUnavailable(),
        bluetooth.generation.load());
    Queued queued;
    // 1周4件までに制限し、補間と期限監視の処理時間を残す。
    for (int count = 0; count < 4 && xQueueReceive(commands, &queued, 0) == pdTRUE; ++count) {
        if ((queued.wifi && network.connected.load() && queued.session == network.generation.load()) ||
            (!queued.wifi && !network.connected.load() && bluetooth.connected.load() &&
                queued.session == bluetooth.generation.load())) {
            if (queued.isDeviceInfo) {
                handleDeviceInfo(queued.deviceInfoData, queued.deviceInfoLength);
            }
            else {
                handle(queued.command, !vmStopRequested);
            }
        }
    }
    now = millis();
    if ((bluetooth.connected.load() || network.connected.load()) && now - lastCommand > HEARTBEAT_TIMEOUT_MS) {
        vmStopRequested = true;
        if (servos.anyArmed() || streamHz) {
            servos.allOff();
            stopTelemetry();
            status(Protocol::CODE_NOT_ARMED, 0);
        }
        network.stopControl();
    }
    const int failedChannel = servos.tick(now);
    if (failedChannel >= 0) {
        status(Protocol::CODE_IO_ERROR, 0, failedChannel);
    }
    if ((bluetooth.connected.load() || network.connected.load()) && streamHz && now - lastSample >= 1000 / streamHz) {
        lastSample = now;
        publishDeviceTelemetry(now);
    }
    network.tick();
    runBytecode(vmStopRequested);
    delay(2);
}
