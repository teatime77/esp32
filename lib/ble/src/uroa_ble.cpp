/**
 * @file uroa_ble.cpp
 * @brief BLE接続、指令受信、状態通知と認証済み設定転送を管理する。
 */
#include "uroa_ble.h"

namespace uroa {
    /**
     * @brief 設定転送の所有元を保持する。
     * @param owner 稼働中保持する所有元。
     */
    BleTransport::Configuration::Configuration(BleTransport &owner) : owner_(owner) {
    }

    /**
     * @brief 通信開始前のBLEオブジェクトを構築する。
     */
    BleTransport::BleTransport() : configuration_(*this) {
    }

    /**
     * @brief 設定とコールバックを保持し、BLEを開始する。
     * @param config 教材側から渡す設定。
     * @param callbacks 稼働中保持するコールバック。
     */
    void BleTransport::begin(const Config &config, const Callbacks &callbacks) {
        if (running_) {
            return;
        }
        config_ = config;
        callbacks_ = callbacks;
        start();
    }

    /**
     * @brief GATTサービスと広告を開始する。既に稼働中なら何もしない。
     */
    void BleTransport::start() {
        if (running_) {
            return;
        }
        stopping_ = false;
        NimBLEDevice::init(config_.name.c_str());
        if (!NimBLEDevice::setMTU(config_.preferredMtu)) {
            Serial.println("BLE preferred MTU setup FAILED");
        }
        NimBLEDevice::setSecurityAuth(true, true, true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
        NimBLEDevice::setSecurityPasskey(config_.passkey);
        Serial.printf("BLE setup pairing code: %06lu\n", static_cast<unsigned long>(config_.passkey));
        auto server = NimBLEDevice::createServer();
        // コールバックはこのオブジェクトの一部。NimBLEのdeinit時にdeleteさせない。
        server->setCallbacks(this, false);
        auto service = server->createService(config_.serviceUuid.c_str());
        auto command = service->createCharacteristic(config_.commandUuid.c_str(), NIMBLE_PROPERTY::WRITE, 10);
        command->setCallbacks(this);
        if (config_.deviceInfoUuid.length() > 0) {
            // 機器情報は専用コールバックへ渡し、Commandとして解釈しない。
            deviceInfo_ = service->createCharacteristic(
                config_.deviceInfoUuid.c_str(), NIMBLE_PROPERTY::WRITE, DeviceInfoMaxLength);
            deviceInfo_->setCallbacks(this);
        }
        telemetry_ = service->createCharacteristic(
            config_.telemetryUuid.c_str(), NIMBLE_PROPERTY::NOTIFY, config_.telemetryMaxLength);
        status_ = service->createCharacteristic(
            config_.statusUuid.c_str(), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 12);
        if (callbacks_.configure) {
            configuration_.reset();
            auto config = service->createCharacteristic(config_.configUuid.c_str(),
                NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN, 384);
            auto result = service->createCharacteristic(config_.resultUuid.c_str(),
                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN, 512);
            config->setCallbacks(&configuration_);
            result->setCallbacks(&configuration_);
        }
        running_ = true;
        if (callbacks_.started) {
            callbacks_.started(); // サービス公開前に初期Statusを設定する。
        }
        service->start();
        auto advertising = NimBLEDevice::getAdvertising();
        advertising->enableScanResponse(true);
        if (!advertising->addServiceUUID(config_.serviceUuid.c_str()) || !advertising->setName(config_.name.c_str()) ||
            !advertising->start()) {
            Serial.println("BLE advertising setup FAILED");
        }
        else {
            Serial.printf("BLE advertising: %s\n", config_.name.c_str());
        }
    }

    /**
     * @brief 広告の再開を抑止してBLEを終了する。
     */
    void BleTransport::stop() {
        if (!running_) {
            return;
        }
        stopping_ = true; // deinitによる切断では広告を再開しない。
        NimBLEDevice::deinit(true);
        connected = false;
        mtu = DefaultAttMtu;
        telemetry_ = status_ = nullptr;
        deviceInfo_ = nullptr;
        running_ = false;
    }

    /**
     * @brief 接続世代を更新して接続を通知する。
     */
    void BleTransport::onConnect(NimBLEServer *, NimBLEConnInfo &info) {
        mtu = info.getMTU();
        ++generation;
        connected = true;
        if (callbacks_.session) {
            callbacks_.session(true, generation);
        }
        Serial.println("BLE connected");
        Serial.printf("BLE setup pairing code: %06lu\n", static_cast<unsigned long>(config_.passkey));
    }

    /**
     * @brief 切断を通知し、通常の切断なら広告を再開する。
     * @param reason 切断理由または制御の停止理由。
     */
    void BleTransport::onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason) {
        connected = false;
        mtu = DefaultAttMtu;
        if (stopping_) {
            return;
        }
        ++generation;
        if (callbacks_.session) {
            callbacks_.session(false, generation);
        }
        Serial.printf("BLE disconnected: reason=%d\n", reason);
        Serial.println(
            NimBLEDevice::startAdvertising() ? "BLE advertising restarted" : "BLE advertising restart FAILED");
    }

    /** @brief ATT MTU交渉の結果を公開する。@param value 合意したMTU。 */
    void BleTransport::onMTUChange(uint16_t value, NimBLEConnInfo &) {
        mtu = value;
    }

    /**
     * @brief BLE書き込みを受け、指令または認証済み設定転送を処理する。
     * @param characteristic 指令を受けたBLE Characteristic。
     */
    void BleTransport::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) {
        auto value = characteristic->getValue();
        // バッファはコールバック中だけ有効。受け手は即座にキューへコピーする。
        if (characteristic == deviceInfo_) {
            if (callbacks_.deviceInfo) {
                callbacks_.deviceInfo(reinterpret_cast<const uint8_t *>(value.data()), value.size(), generation.load());
            }
            return;
        }
        if (callbacks_.command) {
            callbacks_.command(reinterpret_cast<const uint8_t *>(value.data()), value.size(), generation);
        }
    }

    /**
     * @brief 最新のStatus値を設定し、接続中なら通知する。
     * @details データを指定して送信バッファーへコピーし、連続する応答が後の値へ置き換わるのを防ぐ。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     */
    void BleTransport::publishStatus(const uint8_t *data, size_t length) {
        if (!running_ || stopping_ || !status_) {
            return;
        }
        status_->setValue(data, length);
        if (connected) {
            status_->notify(data, length);
        }
    }

    /**
     * @brief 接続中のクライアントへTelemetryを通知する。
     * @details 無引数のnotify()は更新通知が集約され得るため、各パケットのデータを明示して送る。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 送信APIが受理したらtrue。未接続・通知不可・送信失敗はfalse。相手の受信は保証しない。
     */
    bool BleTransport::publishTelemetry(const uint8_t *data, size_t length) {
        if (!connected || !running_ || stopping_ || !telemetry_ || length > config_.telemetryMaxLength ||
            length + NotificationHeaderLength > mtu.load()) {
            return false;
        }
        telemetry_->setValue(data, length);
        return telemetry_->notify(data, length);
    }

    /**
     * @brief 設定JSONの受信内容と応答を初期状態へ戻す。
     */
    void BleTransport::Configuration::reset() {
        input_ = "";
        reply_ = "{\"state\":\"idle\"}";
    }

    /**
     * @brief BLE書き込みを受け、指令または認証済み設定転送を処理する。
     * @param c 設定転送に使うBLE Characteristic。
     * @param peer 暗号化・認証状態を調べる接続情報。
     */
    void BleTransport::Configuration::onWrite(NimBLECharacteristic *c, NimBLEConnInfo &peer) {
        if (!peer.isEncrypted() || !peer.isAuthenticated()) {
            return;
        }
        auto value = c->getValue();
        if (value.size() < 1) {
            return;
        }
        // 先頭0=初期化、1=続き、2=確定。既存diagramの分割JSON転送を維持する。
        if (uint8_t(value[0]) == 0) {
            input_ = "";
        }
        if (value.size() > 1 && input_.length() + value.size() - 1 < 384) {
            for (size_t i = 1; i < value.size(); ++i) {
                input_ += char(value[i]);
            }
        }
        else if (value.size() > 1) {
            input_ = "";
            reply_ = "{\"error\":\"Request too large\"}";
            return;
        }
        if (uint8_t(value[0]) != 2) {
            return;
        }
        reply_ = owner_.callbacks_.configure(input_);
        input_ = "";
    }

    /**
     * @brief 暗号化・認証済みの相手に設定結果を返す。
     * @param c 設定転送に使うBLE Characteristic。
     * @param peer 暗号化・認証状態を調べる接続情報。
     */
    void BleTransport::Configuration::onRead(NimBLECharacteristic *c, NimBLEConnInfo &peer) {
        c->setValue(peer.isEncrypted() && peer.isAuthenticated() ? reply_.c_str() : "{}");
    }
}
