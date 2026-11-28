/**
 * @file main.cpp
 * @brief diagramのBLE/Wi-Fi指令でサーボを操作し、登録されたIMU・距離の測定値を返す。
 * @details 通常指令はキュー、一括Python転送はmutex付き受信領域へコピーする。検証・VM・I2Cはloop()で行う。
 * HC-SR04のGPIOと同期測定は専用タスクだけが操作し、測定結果をキューでloop()へ渡す。
 * 通常通信の受信元・接続世代の照合と通知配送はcontrol_transport.h/.cppへ委譲する。
 * BLEのRPC登録・整数要求キューはbytecode_rpc.h/.cppが所有し、VM開始時に対応するnative表を選ぶ。
 * 通常指令の受理・opcode振分け・応答生成はcontrol_rpc.generated.h/.cpp、ここには機能本体と状態取得を置く。
 */
#include <Arduino.h>
#include <Wire.h>
#include <uroa_ble.h>
#include <uroa_imu.h>
#include <uroa_protocol.h>
#include <uroa_wire.generated.h>
#include <uroa_servo.h>
#include <uroa_ultrasonic.h>
#include <atomic>
#include <cstring>
#include <math.h>
#include "config.h"
#include "device_name.h"
#include "network.h"
#include "control_transport.h"
#include "control_rpc.generated.h"
#include "bytecode_receiver.h"
#include "bytecode_vm.h"
#include "bytecode_debugger.h"
#include "device_native.h"
#include "bytecode_rpc.h"

namespace {
    using uroa::Pca9685Servos;
    using uroa::Protocol;
    using RpcCode = uroa::rpc::StatusCode;

    constexpr size_t DeviceInfoMaxLength = uroa::BleTransport::DeviceInfoMaxLength; ///< 機器情報の最大バイト数。
    constexpr size_t DeviceTelemetryMaxLength = Protocol::DeviceTelemetryHeaderLength +
        DeviceInfoMaxLength * Protocol::IMU6AxisRecordLength; ///< 最大20個のIMUレコードとヘッダー。
    constexpr uint16_t NotificationHeaderLength =
        uroa::BleTransport::NotificationHeaderLength; ///< ATT通知のヘッダー長。

    /**
     * @brief diagramのDeviceTypeと共通の機器種別。宣言の受付はドライバーの実装・機器の存在を示さない。
     */
    enum class DeviceType : uint8_t {
        IMU6Axis = 0,
        IMU9Axis = 1,
        UltrasonicDistanceSensor = 2,
        Camera = 3,
        ServoMotor = 128,
    };

    /**
     * @brief 受信済み機器情報。channelとmodeはサーボの場合だけ使う。
     */
    struct DeviceInfoEntry {
        DeviceType type; ///< Web側のDeviceTypeと共通の種別。handleDeviceInfo()が全件検証する。
        uint8_t channel = 255; ///< サーボのCH0〜15。他の種別では未使用。
        uint8_t mode = 0; ///< サーボの0=角度、1=速度。他の種別では未使用。
    };

    DeviceInfoEntry deviceInfos[DeviceInfoMaxLength]; ///< loop()だけが更新する、受信順の機器一覧。
    size_t deviceInfoCount = 0; ///< 一覧の有効件数。Wi-Fi切り替え時は維持し、終了・飽和時にloop()で消去する。
    bool deviceInfoReceived = false; ///< STREAM開始前の機器登録状態。loop()だけが更新する。

    uroa::BleTransport bluetooth; ///< 稼働中保持するBLEサーバー。
    ControlTransport controlTransport(bluetooth, network); ///< 通常指令の接続照合と通知配送の共通窓口。
    BytecodeReceiver bytecodeReceiver; ///< loop()で分割受信を検証し、確定後にVMへ渡す。
    BytecodeVM bytecodeVm; ///< loop()だけが操作するVM。受信器の確定バッファを借用する。
    BytecodeDebugger bytecodeDebugger(bytecodeReceiver, bytecodeVm, controlTransport); ///< 両経路のデバッグ要求・命令フラグ・停止値の所有者。
    ControlSession vmSession{}; ///< 最後の一括開始またはVM_STARTの受信元。loop()が更新し、別接続での実行継続を禁止する。
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

    /**
     * @brief 超音波測定タスクからloop()へ渡す、測定要求の世代と距離。
     */
    struct UltrasonicSample {
        uint32_t request = 0; ///< 測定開始時の要求値。loop()が現世代の要求と照合する。
        uint32_t measuredAt = 0; ///< 測定を終えたmillis。VMは古い結果を数値として返さない。
        uint16_t distanceMm = Protocol::InvalidDistanceMm; ///< 測定した距離（mm）。未測定・失敗では無効値を保持する。
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

    /**
     * @brief 受信指令と、その受信元・接続世代を保持する。
     */
    struct Queued {
        Protocol::Command command; ///< 生成codecで復号した通常指令の値。isDeviceInfo=falseのときに使う。
        ControlTransport::Session session{}; ///< 受信元と接続世代。キュー取出し後も現接続へ照合する。
        bool isDeviceInfo = false; ///< trueなら通常指令ではなく機器一覧の置換要求。
        uint8_t deviceInfoLength = 0; ///< deviceInfoDataの有効バイト数。0は一覧消去。
        uint8_t deviceInfoData[DeviceInfoMaxLength]{}; ///< 通信コールバックでコピーした機器一覧。loop()が検証する。
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

    /**
     * @brief 専用タスクのGPIO初期化結果を読む。
     * @return 測定要求を受け取れるならtrue。
     */
    bool nativeDistanceReady();
    /**
     * @brief VMの距離要求だけを変更する。
     * @param enabled 有効にするか。
     */
    void enableNativeDistance(bool enabled);
    /**
     * @brief 最新距離をVMへ渡す。
     * @param result mm。
     * @return 成功・待機・失敗。
     */
    BytecodeVM::Error readNativeDistance(float &result);

    DeviceNative nativeDevices(Wire, servos, imu, {nativeDistanceReady, enableNativeDistance, readNativeDistance}); ///< VMが使う機器ハンドル・待機・表示の所有者。setup()/loop()で操作する。
    BytecodeVM::NativeApi nativeApi = device_api::makeNativeApi(nativeDevices); ///< setup()でVMへ登録する静的な表。
    BytecodeRpc bytecodeRpc(bytecodeVm, nativeApi, controlTransport); ///< loop()で操作するRPC登録と整数要求キュー。

    /**
     * @brief 確定バッファの置換で旧VMを停止し、開始済みのVMを最大128命令ずつ実行する。
     * @param stopped このloop()でSTOP・接続変更・通信期限切れを処理した場合true。
     * @details 受信器のバッファ置換後は、旧バッファを使うtick()より前にstart()またはstop()する。
     *
     * 一括COMMITの開始フラグがあれば全体検証・PC設定後に開始し、専用Resultへ開始結果を保持する。
     * 保持専用の転送とprogram.startによる明示開始、program.stopによるVMと所有機器の停止も利用できる。
     * 通常指令の操作番号・受理条件はdevice.protoと生成した振分けを参照する。Web側のdevice-connection.tsが
     * 最後の転送・現在の接続・取消を確認する。開始ACKはstart()の結果までで、終了通知ではない。
     * 1周最大128命令を実行し、通常の指令・サーボ・通信・停止処理へ制御を返す。
     * STOP、切断、Wi-Fi切替、接続変更、通信キュー飽和、5秒の通信期限切れでもVMを停止する。
     * 再接続やPINGで自動再開しない。保持コードはVM_STARTで再実行できる。
     * VM_STARTを受けた通信経路と接続世代をvmSessionへ記録し、実行中もControlTransportで照合する。
     * プログラム・デバッグの通信は両経路で同じ受信器・VMを使い、受信結果の正本は各専用窓口に分ける。
     * Pythonのsleep待機でもloop()を継続し、各周回の停止・通信処理を優先する。待機の仕様は
     * device_native.cppのfunction_sleepとbytecode_vm.cppのcallNativeを参照する。
     *
     * 一括転送のResultは保持成功または開始成功を区別する。正常終了・実行時エラーはデバッグEventとシリアルで確認する。
     *
     * ## 所有権と確認範囲
     *
     * VMは受信器の確定バッファを借用し、コピーしない。受信器の置換後は、loop()がVMを進める前に世代を照合し、新しいstart()かstop()を呼ぶ。BLEコールバックからVMを操作しない。未完了・不正な転送は確定済みバッファを置き換えず、以前のVMの実行を直接開始し直さない。
     * デバッグ用bit7の書換え・Paused通知・停止Snapshotはbytecode_debugger.cppを参照する。
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
        const bool connected = controlTransport.isCurrent(vmSession);
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
         * 番号は大域変数の値スロット。機器は不透明ハンドル、GC本体はclassIdだけを表示し、生アドレスを出さない。
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
                else if (value.type == BytecodeVM::Type::HeapReference) {
                    Serial.printf("Object(class=%u)\n", unsigned(value.classId));
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

    /**
     * @brief loop()から通知と超音波測定要求を停止し、旧サンプルを無効化する。
     */
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
     * @brief 指定CHの指令状態を取得する。wire変換は生成側が行う。
     * @param channel CHまたは全体通知の番号。
     * @return 機器フラグ・角度cdeg・速度%・残り時間。
     */
    uroa::rpc::ControlStatusState controlStatusState(uint8_t channel) {
        uroa::rpc::ControlStatusState snapshot{};
        const auto *state = channel < uroa::rpc::StatusPacketLayout::ChannelCount ? &servos.states[channel] : nullptr;
        snapshot.flags = stateFlags();
        if (state) {
            snapshot.speedMode = state->mode == Pca9685Servos::Mode::Speed;
            snapshot.speedPercent = state->speed;
            snapshot.current = state->current;
            snapshot.target = state->target;
        }
        snapshot.remaining = servos.remaining(channel, millis());
        return snapshot;
    }

    /**
     * @brief 完成済みのStatusを現在のBLE/Wi-Fi経路へ通知する。
     * @param bytes 生成したバイト列。
     * @param length バイト数。
     */
    void publishControlReply(const uint8_t *bytes, size_t length) {
        controlTransport.publishStatus(bytes, length);
    }

    /**
     * @brief 通常形式が有効な指令の受理時刻を、細部の範囲検査前に記録する。
     */
    void acceptControlCommand() {
        lastCommand = millis();
    }

    constexpr uroa::rpc::ControlReplyService controlReplies{
        controlStatusState, publishControlReply, acceptControlCommand}; ///< loop()の共有状態と配送窓口。

    /**
     * @brief 自発通知・優先STOPにも通常応答と同じ生成Statusを使う。
     * @param code 受付結果。
     * @param sequence ACK対象。0は自発通知。
     * @param channel CHまたは全体。
     */
    void status(uint8_t code, uint16_t sequence, uint8_t channel = 255) {
        uroa::rpc::publishControlStatus(controlReplies, code, sequence, channel);
    }

    /**
     * @brief 通信タスクで指令を生成codecの値へ復号・コピーし、STOPを優先フラグへ渡す。
     * @param data コールバック中だけ有効な受信データ。
     * @param length 受信バイト数。
     * @param session 受信元の通信経路と接続世代。
     * @details I2Cを操作しない。受信時とキューから実行する直前に接続・制御権を照合する。
     * STOPも旧接続からは受け付けず、有効な接続のSTOPは従来どおり通常キューを迂回する。
     */
    void receive(const uint8_t *data, size_t length, ControlTransport::Session session) {
        if (!controlTransport.isCurrent(session)) {
            return;
        }
        if (session.source == ControlTransport::Source::Wifi && length != uroa::rpc::CommandPacketLayout::ByteLength) {
            return;
        }
        if (Protocol::urgentStop(data, length)) {
            urgentSeq.store(Protocol::read16(data + uroa::rpc::CommandPacketLayout::sequence));
            urgentStop.store(true);
            return;
        }
        Queued queued{};
        queued.command = Protocol::command(data, length);
        queued.session = session;
        if (xQueueSend(commands, &queued, 0) != pdTRUE) {
            resetDeviceInfo.store(true);
            mustStop.store(true);
            if (session.source == ControlTransport::Source::Wifi) {
                network.stopControl();
            }
        }
    }

    /**
     * @brief BLE/Wi-Fiタスクの機器情報を現在の接続世代とともにキューへコピーする。
     * @param data コールバック中だけ有効な受信データ。
     * @param length 受信バイト数。0は機器一覧の消去。
     * @param session 受信元の通信経路と接続世代。
     * @details I2Cと機器情報の解釈は行わない。キュー飽和時は停止要求を出し、Wi-Fi制御権を解除する。
     * コールバックへの配送・GATT書込みの成功は一覧の内容検証完了を意味しない。
     */
    void receiveDeviceInfo(const uint8_t *data, size_t length, ControlTransport::Session session) {
        if (!controlTransport.isCurrent(session) || length > DeviceInfoMaxLength) {
            return;
        }
        Queued queued{};
        queued.session = session;
        queued.isDeviceInfo = true;
        queued.deviceInfoLength = static_cast<uint8_t>(length);
        if (length > 0) {
            std::memcpy(queued.deviceInfoData, data, length);
        }
        if (xQueueSend(commands, &queued, 0) != pdTRUE) {
            resetDeviceInfo.store(true);
            mustStop.store(true);
            if (session.source == ControlTransport::Source::Wifi) {
                network.stopControl();
            }
        }
    }

    /**
     * @brief 機器情報を全件検証し、成功時だけ受信順の一覧を置き換える。
     * @param data 種別1バイト、サーボの場合は続けてCH・モードを持つ受信データ。
     * @param length 受信バイト数。0から20。
     * @details loop()から呼ぶ。登録のみで機器初期化・サーボのARM・計測開始は行わない。
     * BLE/Wi-Fiとも同じ生一覧を全件検証する。空一覧で消去、不正な内容では以前の一覧を保持する。
     * Wi-Fi切替直後もWeb側が一覧を再送し、置換で停止したSTREAMを新しく要求する。
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
            const auto tag = uroa::wire::readDeviceTag(data + offset);
            const size_t bytes = uroa::wire::DeviceInfoRecordsRecordLength(tag.deviceType);
            if (bytes == 0 || bytes > length - offset) {
                Serial.println("DeviceInfo rejected: invalid record length or type");
                return;
            }
            DeviceInfoEntry entry{};
            entry.type = static_cast<DeviceType>(tag.deviceType);
            if (entry.type == DeviceType::ServoMotor) {
                const auto record = uroa::wire::readDeviceServo(data + offset);
                if (!uroa::wire::validDeviceServo(record)) {
                    Serial.println("DeviceInfo rejected: invalid servo record");
                    return;
                }
                entry.channel = record.channel;
                entry.mode = record.mode;
            }
            offset += bytes;
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

    /**
     * @brief VMが使う測定タスクの準備状態を確認する。
     * @return 準備済みならtrue。
     */
    bool nativeDistanceReady() {
        return ultrasonicReady.load() && ultrasonicSamples != nullptr;
    }

    /**
     * @brief VMの距離要求を切り替え、STREAMと合成する。
     * @param enabled 有効にするか。
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

    /**
     * @brief 最新の測定値をmmで返す。
     * @param result mm。
     * @return 成功・NativePending・NativeFailure。
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
        if (!controlTransport.canPublishTelemetry(length)) {
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
        controlTransport.publishTelemetry(packet, written);
    }

    /**
     * @brief サーボ操作の結果を通信の応答コードへ変換する。
     * @param result 共通サーボライブラリーの結果。
     * @return Protocolの受付結果コード。
     */
    RpcCode resultCode(Pca9685Servos::Result result) {
        switch (result) {
            case Pca9685Servos::Result::Ok:
                return RpcCode::CODE_OK;
            case Pca9685Servos::Result::Range:
                return RpcCode::CODE_RANGE;
            case Pca9685Servos::Result::Unavailable:
                return RpcCode::CODE_HARDWARE;
            case Pca9685Servos::Result::NotArmed:
                return RpcCode::CODE_NOT_ARMED;
            case Pca9685Servos::Result::WrongMode:
                return RpcCode::CODE_MODE;
            default:
                return RpcCode::CODE_IO_ERROR;
        }
    }

    /**
     * @brief 指定CHを角度モードで有効化する。
     * @param channel 検証済みCH。
     * @param angle 受信角度を度へ変換した値。範囲はドライバーが検査する。
     * @return 機器操作の結果。
     */
    RpcCode armServoAngle(uint8_t channel, float angle) {
        return resultCode(servos.armAngle(channel, angle));
    }

    /**
     * @brief 指定CHを速度モードで有効化する。
     * @param channel 検証済みCH。
     * @return 機器操作の結果。
     */
    RpcCode armServoSpeed(uint8_t channel) {
        return resultCode(servos.armSpeed(channel));
    }

    /**
     * @brief 角度の移動補間を開始する。
     * @param channel 検証済みCH。
     * @param angle 度単位の目標。
     * @param duration 移動時間ms。
     * @return ドライバーが範囲・有効状態を検査した結果。
     */
    RpcCode moveServo(uint8_t channel, float angle, uint16_t duration) {
        return resultCode(servos.moveAngle(channel, angle, duration, millis()));
    }

    /**
     * @brief 方向付き速度指令を設定する。
     * @param channel 検証済みCH。
     * @param percent 符号と単位を変換した速度指令%。
     * @return ドライバーが範囲・モードを検査した結果。
     */
    RpcCode setServoSpeed(uint8_t channel, float percent) {
        return resultCode(servos.setSpeed(channel, percent));
    }

    /**
     * @brief 指定CHの出力を無効化する。
     * @param channel 検証済みCH。
     * @return 機器操作の結果。
     */
    RpcCode disableServo(uint8_t channel) {
        return resultCode(servos.disable(channel));
    }

    /**
     * @brief 通常経路の全サーボ停止を行う。優先STOPのキュー処理はloop()に残す。
     * @return 停止指令の受付成功。
     */
    RpcCode stopServos() {
        servos.allOff();
        return RpcCode::CODE_OK;
    }

    /**
     * @brief VM指令の共通停止と所有イメージ世代の更新を行う。
     * @details loop()だけで呼ぶ。保持バッファの置換後に旧イメージを読み進めない。
     */
    void resetVmForCommand() {
        bytecodeDebugger.invalidateSnapshot();
        bytecodeVm.stop();
        vmRevision = bytecodeReceiver.programRevision;
        reportedVmState = BytecodeVM::State::Stopped;
    }

    /**
     * @brief 所有コードのVMを開始する。成功は初期化までを示す。
     * @param context 受信元と停止優先の状態。
     * @return 制御権・開始条件とVM初期化の結果。
     */
    RpcCode startProgram(const ControlRpcContext &context) {
        if (!controlTransport.isCurrent(context.session)) {
            return RpcCode::CODE_NOT_ARMED;
        }
        if (!context.allowVmStart || mustStop.load() || urgentStop.load() || bytecodeReceiver.deploymentPending) {
            return RpcCode::CODE_BUSY;
        }
        resetVmForCommand();
        if (!bytecodeReceiver.program || bytecodeReceiver.programLength == 0) {
            return RpcCode::CODE_NOT_ARMED;
        }
        vmSession = context.session;
        const bool started =
            bytecodeRpc.bindProgram(bytecodeReceiver.program.get(), bytecodeReceiver.programLength, context.session) &&
            bytecodeVm.start(bytecodeReceiver.program.get(), bytecodeReceiver.programLength);
        // time()の起点はVMの開始成功直後。仕様はdevice_native.cppのfunction_timeを参照する。
        if (started) {
            nativeDevices.startProgramClock();
        }
        return started ? RpcCode::CODE_OK : RpcCode::CODE_FORMAT;
    }

    /**
     * @brief VM・公開要求と一括開始予約を停止する。保持コードは残す。
     * @param context 受信元と世代。
     * @return 現在の制御権からの停止なら成功。
     */
    RpcCode stopProgram(const ControlRpcContext &context) {
        if (!controlTransport.isCurrent(context.session)) {
            return RpcCode::CODE_NOT_ARMED;
        }
        resetVmForCommand();
        bytecodeRpc.stop();
        bytecodeReceiver.cancelDeployment();
        return RpcCode::CODE_OK;
    }

    /**
     * @brief 登録済みセンサーの共有通知を開始・停止する。
     * @param rate 生成側が受理した通知Hz。
     * @return 機器登録・測定準備・現在の通信容量を検査した結果。
     */
    RpcCode setStreamRate(uint16_t rate) {
        if (rate) {
            if (!deviceInfoReceived) {
                Serial.println("DeviceTelemetry requires DeviceInfo before STREAM");
                return RpcCode::CODE_FORMAT;
            }
            const size_t length = deviceTelemetryLength();
            if (length == 0) {
                Serial.println("DeviceTelemetry unavailable: unsupported sensor type");
                return RpcCode::CODE_HARDWARE;
            }
            if ((hasSensor(DeviceType::IMU6Axis) && !imu.available) ||
                (hasSensor(DeviceType::UltrasonicDistanceSensor) && !ultrasonicReady.load())) {
                return RpcCode::CODE_HARDWARE;
            }
            if (!controlTransport.canPublishTelemetry(length)) {
                Serial.printf("DeviceTelemetry requires ATT MTU >= %u; current=%u\n",
                    static_cast<unsigned>(length + NotificationHeaderLength),
                    static_cast<unsigned>(bluetooth.mtu.load()));
                return RpcCode::CODE_RANGE;
            }
        }
        streamHz = rate;
        setUltrasonicSampling(rate != 0 && hasSensor(DeviceType::UltrasonicDistanceSensor));
        lastSample = millis();
        return RpcCode::CODE_OK;
    }

    constexpr uroa::rpc::ControlServices controlServices{
        {armServoAngle, armServoSpeed, moveServo, setServoSpeed, disableServo, stopServos}, {startProgram, stopProgram},
        {setStreamRate}, controlReplies}; ///< 通信の位置・番号を含めず、型付きの機能サービスを登録する。

    /**
     * @brief loop()の受信元・停止優先状態を生成した振分けへ渡す。
     * @param command 受信時の生成codecで復号した値。
     * @param allowVmStart 停止要求を処理した周回ではfalse。
     * @param session 受信元と接続世代。VM操作の直前にも再照合する。
     */
    void handle(const Protocol::Command &command, bool allowVmStart, const ControlSession &session) {
        const ControlRpcContext context{session, allowVmStart};
        uroa::rpc::dispatchControl(command, context, controlServices);
    }

}

/**
 * @brief I2C機器、超音波測定タスク、指令キュー、Wi-FiとBLEを開始する。
 * @details 起動時にサーボを自動ARMしない。接続変更時も停止を予約する。
 * 表示名はdevice_name.cppの仕様に従って共通生成し、MAC取得失敗時は通信を開始しない。
 */
void setup() {
    Serial.begin(115200);
    delay(200);
    const String deviceName = makeDeviceName();
    if (deviceName.isEmpty()) {
        Serial.println("Device name setup FAILED: factory MAC unavailable");
        while (true) {
            delay(1000);
        }
    }
    Serial.printf("Device name: %s\n", deviceName.c_str());
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
    if (!bytecodeRpc.begin()) {
        Serial.println("RPC queue allocation failed");
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
        receive(data, length, {ControlTransport::Source::Wifi, generation});
    };
    wifiCallbacks.deviceInfo = [](const uint8_t *data, size_t length, uint32_t generation) {
        receiveDeviceInfo(data, length, {ControlTransport::Source::Wifi, generation});
    };
    wifiCallbacks.bytecode = [](const uint8_t *data, size_t length, uint32_t generation) {
        const ControlSession session{ControlSource::Wifi, generation};
        if (controlTransport.isCurrent(session)) {
            bytecodeReceiver.enqueue(data, length, session);
        }
    };
    wifiCallbacks.debug = [](const uint8_t *data, size_t length, uint32_t generation) {
        const ControlSession session{ControlSource::Wifi, generation};
        if (controlTransport.isCurrent(session)) {
            bytecodeDebugger.enqueue(data, length, session);
        }
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
    beginNetwork(wifiCallbacks, deviceName);

    uroa::BleTransport::Config bleConfig;
    bleConfig.name = deviceName;
    bleConfig.serviceUuid = SERVICE_UUID;
    bleConfig.commandUuid = COMMAND_UUID;
    bleConfig.telemetryUuid = TELEMETRY_UUID;
    bleConfig.statusUuid = STATUS_UUID;
    bleConfig.configUuid = CONFIG_UUID;
    bleConfig.resultUuid = RESULT_UUID;
    bleConfig.deviceInfoUuid = DEVICE_INFO_UUID;
    bleConfig.bytecodeUuid = BYTECODE_UUID;
    bleConfig.bytecodeResultUuid = BYTECODE_RESULT_UUID;
    bleConfig.debugControlUuid = DEBUG_CONTROL_UUID;
    bleConfig.debugResultUuid = DEBUG_RESULT_UUID;
    bleConfig.debugEventUuid = DEBUG_EVENT_UUID;
    bleConfig.debugDataUuid = DEBUG_DATA_UUID;
    bleConfig.rpcControlUuid = uroa::wire::GattUuidValues::rpcControl;
    bleConfig.rpcResultUuid = uroa::wire::GattUuidValues::rpcResult;
    bleConfig.rpcEventUuid = uroa::wire::GattUuidValues::rpcEvent;
    bleConfig.telemetryMaxLength = DeviceTelemetryMaxLength;
    // 大きいTelemetryに必要な既存MTUも維持し、コードは最大244バイトのWriteへ制限する。
    bleConfig.preferredMtu = DeviceTelemetryMaxLength + NotificationHeaderLength > 247
        ? DeviceTelemetryMaxLength + NotificationHeaderLength : 247;
    bleConfig.passkey = PAIRING_CODE;
    bytecodeReceiver.begin([](const uint8_t *data, size_t length, const ControlSession &session) {
        controlTransport.publishBytecodeResult(data, length, session);
    }, [](const ControlSession &session) {
        return controlTransport.isCurrent(session);
    });
    bytecodeDebugger.begin();
    uroa::BleTransport::Callbacks bleCallbacks;
    bleCallbacks.command = [](const uint8_t *data, size_t length, uint32_t generation) {
        receive(data, length, {ControlTransport::Source::Ble, generation});
    };
    bleCallbacks.deviceInfo = [](const uint8_t *data, size_t length, uint32_t generation) {
        receiveDeviceInfo(data, length, {ControlTransport::Source::Ble, generation});
    };
    bleCallbacks.bytecode = [](const uint8_t *data, size_t length, uint32_t generation) {
        const ControlSession session{ControlSource::Ble, generation};
        if (controlTransport.isCurrent(session)) {
            bytecodeReceiver.enqueue(data, length, session);
        }
    };
    bleCallbacks.debug = [](const uint8_t *data, size_t length, uint32_t generation) {
        const ControlSession session{ControlSource::Ble, generation};
        if (controlTransport.isCurrent(session)) {
            bytecodeDebugger.enqueue(data, length, session);
        }
    };
    bleCallbacks.session = [](bool, uint32_t) {
        if (!network.connected.load()) {
            resetDeviceInfo.store(true);
        }
        mustStop.store(true);
    };
    bleCallbacks.rpc = [](const uint8_t *data, size_t length, uint32_t generation) {
        const ControlSession session{ControlSource::Ble, generation};
        if (controlTransport.isCurrent(session)) {
            bytecodeRpc.enqueue(data, length, session);
        }
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

/**
 * @brief 停止を優先し、現接続の指令、サーボ補間、期限監視と登録センサーの通知を進める。
 */
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
    const ControlSession session = controlTransport.currentSession();
    if (vmStopRequested) {
        bytecodeRpc.stop();
    }
    bytecodeRpc.process();
    bytecodeReceiver.process(controlTransport.isCurrent(session), session);
    if (bytecodeReceiver.deploymentPending) {
        // 旧イメージを参照するVMを進める前に停止する。新コードの開始はPC設定後に行う。
        bytecodeVm.stop();
        vmRevision = bytecodeReceiver.programRevision;
        // 前回と同じRunning/Errorでも、新しい開始の最初の状態を必ずシリアルへ報告する。
        reportedVmState = BytecodeVM::State::Idle;
    }
    Queued queued;
    // 1周4件までに制限し、補間と期限監視の処理時間を残す。
    for (int count = 0; count < 4 && xQueueReceive(commands, &queued, 0) == pdTRUE; ++count) {
        if (controlTransport.isCurrent(queued.session)) {
            if (queued.isDeviceInfo) {
                handleDeviceInfo(queued.deviceInfoData, queued.deviceInfoLength);
            }
            else {
                handle(queued.command, !vmStopRequested, queued.session);
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
    // デバッグ要求もloop()で処理し、Stopがある周回では設定・値取得を受け付けない。
    // PausedではVMだけを止める。機器出力・補間は維持し、既存のStop・切断・通信期限が停止させる。
    bytecodeDebugger.process(!vmStopRequested && controlTransport.isCurrent(session), session);
    if (vmStopRequested || mustStop.load() || urgentStop.load()) {
        bytecodeRpc.stop();
        bytecodeReceiver.cancelDeployment();
    }
    if (bytecodeReceiver.deploymentPending) {
        // 一括転送のCOMMITだけが開始を要求できる。結果は実行がすぐ終了しても専用Resultに保持する。
        bool success = controlTransport.isCurrent(session) && bytecodeDebugger.installDeployment();
        if (success && bytecodeReceiver.startRequested) {
            vmSession = session;
            success =
                bytecodeRpc.bindProgram(bytecodeReceiver.program.get(), bytecodeReceiver.programLength, session) &&
                bytecodeVm.start(bytecodeReceiver.program.get(), bytecodeReceiver.programLength);
            if (success) {
                nativeDevices.startProgramClock();
            }
        }
        bytecodeReceiver.completeDeployment(success);
    }
    runBytecode(vmStopRequested);
    bytecodeDebugger.publishState();
    delay(2);
}
