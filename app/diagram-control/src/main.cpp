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
#include "config.h"
#include "network.h"

namespace {
    using uroa::Pca9685Servos;
    using uroa::Protocol;

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
    };

    QueueHandle_t commands = nullptr; ///< 通信タスクからloop()へ渡す16件の指令キュー。
    std::atomic<bool> mustStop{false}; ///< 接続変更・キュー飽和時の出力とSTREAMの停止要求。
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
        Protocol::status(packet, code, sequence, stateFlags(), channel, state ? state->current : 0,
            state ? state->target : 0, servos.remaining(channel, millis()));
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
            mustStop.store(true);
            if (wifi) {
                network.stopControl();
            }
        }
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
        else if (operation == Protocol::ARM) {
            result = servos.arm(channel, value, duration);
        }
        else {
            result = servos.move(channel, value, duration, millis());
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
    wifiCallbacks.session = [](bool, uint32_t) {
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
    bleConfig.passkey = PAIRING_CODE;
    uroa::BleTransport::Callbacks bleCallbacks;
    bleCallbacks.command = [](const uint8_t *data, size_t length, uint32_t generation) {
        receive(data, length, generation, false);
    };
    bleCallbacks.session = [](bool, uint32_t) {
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
            handle(queued.command);
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
            uint8_t packet[20];
            Protocol::telemetry(packet, stateFlags(), ++sampleSeq, now, raw);
            if (!network.bleUnavailable()) {
                bluetooth.publishTelemetry(packet, sizeof packet);
            }
            network.publish(packet, sizeof packet);
        }
    }
    network.tick();
    delay(2);
}
