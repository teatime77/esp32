/**
 * @file main.cpp
 * @brief diagramのBLE/Wi-Fi指令でサーボを操作し、IMUの生データを返す。
 * @details 通信タスクは指令をキューへコピーし、I2C・応答・停止・周期処理はloop()に集約する。
 */
#include <Arduino.h>
#include <Wire.h>
#include <uroa_ble.h>
#include <uroa_imu.h>
#include <uroa_protocol.h>
#include <uroa_servo.h>
#include <atomic>
#include <cstring>
#include <math.h>
#include "config.h"
#include "network.h"

namespace {
    using uroa::Pca9685Servos;
    using uroa::Protocol;

    constexpr size_t DeviceInfoMaxLength = uroa::BleTransport::DeviceInfoMaxLength; ///< 機器情報の最大バイト数。
    constexpr size_t DeviceTelemetryMaxLength = Protocol::DeviceTelemetryHeaderLength +
        DeviceInfoMaxLength * Protocol::IMU6AxisRecordLength; ///< 最大20個のIMUレコードとヘッダー。
    constexpr uint16_t NotificationHeaderLength = uroa::BleTransport::NotificationHeaderLength; ///< ATT通知のヘッダー長。

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
    uroa::Mpu6050 imu(Wire); ///< loop()からだけI2C操作するIMU。

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
    uint16_t streamHz = 0; ///< IMU通知周期。0は停止、5/10/20Hzを受け付ける。
    uint16_t sampleSeq = 0; ///< IMU通知sequence。16bitで周回する。
    uint32_t lastCommand = 0; ///< 最後に有効形式の指令を受け付けた時刻（ms）。
    uint32_t lastSample = 0; ///< 最後にIMUを読み取った時刻（ms）。

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
     * @brief 登録されたセンサーのTelemetryに必要な長さを求める。
     * @return 必要なバイト数。未対応のセンサー種別を含む場合は0。
     */
    size_t deviceTelemetryLength() {
        size_t length = Protocol::DeviceTelemetryHeaderLength;
        for (size_t index = 0; index < deviceInfoCount; ++index) {
            if (deviceInfos[index].type == DeviceType::IMU6Axis) {
                length += Protocol::IMU6AxisRecordLength;
            }
            else if (deviceInfos[index].type != DeviceType::ServoMotor) {
                return 0;
            }
        }
        return length;
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
     */
    void handle(const Protocol::Command &command) {
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
        if (operation == Protocol::PING || operation == Protocol::INFO) {
            status(Protocol::CODE_OK, sequence, channel);
            return;
        }
        if (operation == Protocol::STREAM) {
            if (value != 0 && value != 5 && value != 10 && value != 20) {
                status(Protocol::CODE_RANGE, sequence);
                return;
            }
            if (value && !imu.available) {
                status(Protocol::CODE_HARDWARE, sequence);
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
                if (!network.connected.load() && length + NotificationHeaderLength > bluetooth.mtu.load()) {
                    Serial.printf("DeviceTelemetry requires ATT MTU >= %u; current=%u\n",
                        static_cast<unsigned>(length + NotificationHeaderLength),
                        static_cast<unsigned>(bluetooth.mtu.load()));
                    status(Protocol::CODE_RANGE, sequence);
                    return;
                }
            }
            streamHz = value;
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
 * @brief I2C機器を初期化して指令キュー、Wi-FiとBLEを開始する。
 * @details 起動時にサーボを自動ARMしない。接続変更時も停止を予約する。
 */
void setup() {
    Serial.begin(115200);
    delay(200);
    const bool i2cStarted = Wire.begin(SDA_PIN, SCL_PIN);
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
    commands = xQueueCreate(16, sizeof(Queued));
    if (!commands) {
        servos.allOff();
        Serial.println("Queue allocation failed");
        while (true) {
            delay(1000);
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
    bleConfig.telemetryMaxLength = DeviceTelemetryMaxLength;
    bleConfig.preferredMtu = DeviceTelemetryMaxLength + NotificationHeaderLength;
    bleConfig.passkey = PAIRING_CODE;
    uroa::BleTransport::Callbacks bleCallbacks;
    bleCallbacks.command = [](const uint8_t *data, size_t length, uint32_t generation) {
        receive(data, length, generation, false);
    };
    bleCallbacks.deviceInfo = [](const uint8_t *data, size_t length, uint32_t generation) {
        receiveDeviceInfo(data, length, generation);
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
    };
    bluetooth.begin(bleConfig, bleCallbacks);
    Serial.printf("BLE address: %s\n", NimBLEDevice::getAddress().toString().c_str());
}

/** @brief 停止を優先し、現接続の指令、サーボ補間、期限監視とIMU通知を進める。 */
void loop() {
    uint32_t now = millis();
    if (mustStop.exchange(false)) {
        servos.allOff();
        streamHz = 0;
        if (resetDeviceInfo.exchange(false)) {
            deviceInfoCount = 0;
            deviceInfoReceived = false;
        }
        lastCommand = now;
        xQueueReset(commands);
        status(Protocol::CODE_OK, 0);
    }
    if (urgentStop.exchange(false)) {
        servos.allOff();
        xQueueReset(commands);
        lastCommand = now;
        status(Protocol::CODE_OK, urgentSeq.load());
    }
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
                handle(queued.command);
            }
        }
    }
    now = millis();
    if ((bluetooth.connected.load() || network.connected.load()) && now - lastCommand > HEARTBEAT_TIMEOUT_MS) {
        if (servos.anyArmed() || streamHz) {
            servos.allOff();
            streamHz = 0;
            status(Protocol::CODE_NOT_ARMED, 0);
        }
        network.stopControl();
    }
    const int failedChannel = servos.tick(now);
    if (failedChannel >= 0) {
        status(Protocol::CODE_IO_ERROR, 0, failedChannel);
    }
    if ((bluetooth.connected.load() || network.connected.load()) && imu.available && streamHz &&
        now - lastSample >= 1000 / streamHz) {
        lastSample = now;
        uint8_t raw[14];
        if (!imu.readRaw(raw)) {
            streamHz = 0;
            status(Protocol::CODE_IO_ERROR, 0);
        }
        else {
            const size_t length = deviceTelemetryLength();
            if (length == 0 || (!network.connected.load() && length + NotificationHeaderLength > bluetooth.mtu.load())) {
                streamHz = 0;
                status(length == 0 ? Protocol::CODE_HARDWARE : Protocol::CODE_RANGE, 0);
            }
            else {
                uint8_t packet[DeviceTelemetryMaxLength];
                const size_t imuCount = (length - Protocol::DeviceTelemetryHeaderLength) / Protocol::IMU6AxisRecordLength;
                const size_t written = Protocol::deviceTelemetry(
                    packet, sizeof packet, stateFlags(), ++sampleSeq, now, raw, imuCount);
                if (written == length) {
                    if (!network.bleUnavailable()) {
                        bluetooth.publishTelemetry(packet, written);
                    }
                    network.publish(packet, written);
                }
            }
        }
    }
    network.tick();
    delay(2);
}
