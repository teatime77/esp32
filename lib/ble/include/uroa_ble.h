/**
 * @file uroa_ble.h
 * @brief BLE接続、指令受信、状態通知と認証済み設定転送を管理する。
 */
#pragma once
#include <NimBLEDevice.h>
#include <atomic>
#include <functional>

namespace uroa {
    /**
     * @brief ESP32のBLEサーバー。稼働中はインスタンスとコールバックの参照先を保持する。
     * @details ESP32のBLEサーバー。NimBLEは基板全体で1つなので、稼働するインスタンスは1つ。 このオブジェクトとコールバックの参照先はファームウェアの稼働中ずっと保持する。
     */
    class BleTransport : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks {
        public:
        static constexpr uint16_t DefaultAttMtu = 23; ///< ATT MTU交渉前の既定値。
        static constexpr uint16_t NotificationHeaderLength = 3; ///< ATT通知のopcodeとhandleの長さ。
        std::atomic<bool> connected{false}; ///< BLE接続状態。アプリはload()で参照し、更新はライブラリーに任せる。
        std::atomic<uint32_t> generation{0}; ///< 接続・切断で更新する世代番号。アプリはload()で参照する。
        std::atomic<uint16_t> mtu{DefaultAttMtu}; ///< 接続相手と合意したATT MTU。BLEコールバックが更新する。
        static constexpr uint16_t DeviceInfoMaxLength = 20; ///< DeviceInfoの1回の書込上限（バイト）。

        /**
         * @brief BLEの表示名、GATT UUID、ペアリングコード。
         */
        struct Config {
            String name;
            String serviceUuid;
            String commandUuid;
            String telemetryUuid;
            String statusUuid;
            String configUuid;
            String resultUuid;
            String deviceInfoUuid; ///< 機器情報の書込先UUID。空ならCharacteristicを公開しない。
            String bytecodeUuid; ///< バイトコード転送の書込先。結果UUIDと両方指定した場合に公開する。
            String bytecodeResultUuid; ///< バイトコード受付結果の読出先。
            String debugControlUuid; ///< デバッグ要求のWrite窓口。4個のdebug UUID指定時だけ公開する。
            String debugResultUuid; ///< loop()の要求受付結果のRead窓口。
            String debugEventUuid; ///< VM停止状態のRead/Notify窓口。
            String debugDataUuid; ///< 選択したSnapshotチャンクのRead窓口。
            uint32_t passkey = 123456;
            uint16_t preferredMtu = DefaultAttMtu; ///< 希望するATT MTU。実際の値は接続相手との交渉で決まる。
            uint16_t telemetryMaxLength = 20; ///< Telemetry Characteristicの値の最大長。
        };

        /**
         * @brief 指令・機器情報の受信、接続世代の変更、認証済み設定と起動完了の通知先。
         */
        struct Callbacks {
            std::function<void(const uint8_t *, size_t, uint32_t)> command;
            /** @brief BLEタスクから機器情報を渡す。データは呼出中にコピーし、解釈は制御ループで行う。 */
            std::function<void(const uint8_t *, size_t, uint32_t)> deviceInfo;
            /** @brief BLEタスクから20バイト以内の転送フレームを渡す。呼出中にキューへコピーする。 */
            std::function<void(const uint8_t *, size_t, uint32_t)> bytecode;
            /** @brief デバッグ要求をBLEタスクから渡す。受け手は接続世代と内容を即座にコピーする。 */
            std::function<void(const uint8_t *, size_t, uint32_t)> debug;
            std::function<void(bool, uint32_t)> session;
            // Wi-Fi設定JSONの解釈は外部へ委譲する。BLEは認証と分割転送だけを扱う。
            std::function<String(const String &)> configure;
            std::function<void()> started;
        };

        /**
         * @brief 通信開始前のBLEオブジェクトを構築する。
         */
        BleTransport();
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        BleTransport(const BleTransport &) = delete;
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        BleTransport &operator=(const BleTransport &) = delete;
        /**
         * @brief 設定とコールバックを保持し、BLEを開始する。
         * @param config 教材側から渡す設定。
         * @param callbacks 稼働中保持するコールバック。
         */
        void begin(const Config &config, const Callbacks &callbacks);
        /**
         * @brief GATTサービスと広告を開始する。既に稼働中なら何もしない。
         */
        void start();
        /**
         * @brief 広告の再開を抑止してBLEを終了する。
         */
        void stop();

        /**
         * @brief 最新のStatus値を設定し、接続中なら通知する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         */
        void publishStatus(const uint8_t *data, size_t length);
        /**
         * @brief バイトコードの受付結果20バイトをRead値へ反映する。通知は行わない。
         * @param data loop()で組み立てた専用の受付結果。
         * @param length 20バイト以外は更新しない。
         */
        void publishBytecodeResult(const uint8_t *data, size_t length);
        /** @brief デバッグACKをRead値へコピーする。@param data 20バイトの結果。@param length 20。 */
        void publishDebugResult(const uint8_t *data, size_t length);
        /** @brief 停止状態をRead/Notifyへコピーする。@param data 20バイトの状態。@param length 20。 */
        void publishDebugEvent(const uint8_t *data, size_t length);
        /** @brief Snapshotの選択チャンクを設定する。@param data バイト列。@param length 1〜20。 */
        void publishDebugData(const uint8_t *data, size_t length);
        /**
         * @brief 接続中のクライアントへTelemetryを通知する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 送信APIが受理したらtrue。未接続・通知不可・送信失敗はfalse。相手の受信は保証しない。
         */
        bool publishTelemetry(const uint8_t *data, size_t length);

        private:
        /**
         * @brief 認証済みWi-Fi設定JSONの分割受信と応答読出しを処理する。
         */
        class Configuration : public NimBLECharacteristicCallbacks {
            public:
            /**
             * @brief 設定転送の所有元を保持する。
             * @param owner 稼働中保持する所有元。
             */
            explicit Configuration(BleTransport &owner);

            /**
             * @brief 設定JSONの受信内容と応答を初期状態へ戻す。
             */
            void reset();

            private:
            /**
             * @brief BLE書き込みを受け、指令または認証済み設定転送を処理する。
             */
            void onWrite(NimBLECharacteristic *, NimBLEConnInfo &) override;
            /**
             * @brief 暗号化・認証済みの相手に設定結果を返す。
             */
            void onRead(NimBLECharacteristic *, NimBLEConnInfo &) override;
            BleTransport &owner_;
            String input_;
            String reply_;
        } configuration_;

        /**
         * @brief 接続世代を更新して接続を通知する。
         */
        void onConnect(NimBLEServer *, NimBLEConnInfo &) override;
        /** @brief ATT MTU交渉の結果を公開状態へ反映する。@param value 合意したMTU。 */
        void onMTUChange(uint16_t value, NimBLEConnInfo &) override;
        /**
         * @brief 切断を通知し、通常の切断なら広告を再開する。
         * @param reason 切断理由または制御の停止理由。
         */
        void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason) override;
        /**
         * @brief BLE書き込みを受け、指令または認証済み設定転送を処理する。
         */
        void onWrite(NimBLECharacteristic *, NimBLEConnInfo &) override;
        Config config_;
        Callbacks callbacks_;
        NimBLECharacteristic *deviceInfo_ = nullptr; ///< 機器情報の書込窓口。専用コールバックへ渡す。
        NimBLECharacteristic *bytecode_ = nullptr; ///< バイトコード分割転送の書込窓口。
        NimBLECharacteristic *bytecodeResult_ = nullptr; ///< loop()で更新する受付結果のRead窓口。
        NimBLECharacteristic *debugControl_ = nullptr; ///< 要求は専用キューへコピーする。
        NimBLECharacteristic *debugResult_ = nullptr; ///< 要求ACK。停止Notifyで上書きしない。
        NimBLECharacteristic *debugEvent_ = nullptr; ///< 停止情報。Readでも取りこぼしを回復できる。
        NimBLECharacteristic *debugData_ = nullptr; ///< loop()で選択した不変Snapshotのチャンク。
        NimBLECharacteristic *telemetry_ = nullptr;
        NimBLECharacteristic *status_ = nullptr;
        std::atomic<bool> stopping_{false};
        bool running_ = false;
    };
}
