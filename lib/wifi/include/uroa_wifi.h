/**
 * @file uroa_wifi.h
 * @brief Wi-Fi設定、認証、HTTP/WebSocket制御と通信タスクを管理する。
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <esp_http_server.h>
#include <atomic>
#include <functional>

namespace uroa {
    /**
     * @brief 特定のBLE・カメラ実装に依存しないWi-Fi制御サーバー。稼働中は1基板に1個を保持する。
     * @details BLEや特定カメラの実装を知らない、uroa教材向けWi-Fi制御サーバー。 WiFi/mDNSは基板共通資源。1基板で1個を使い、稼働中は破棄しない。 begin()はsetup()で1回、tick()/publish()は制御ループで呼ぶ。
     */
    class WifiNetwork {
        public:
        static constexpr size_t MaxPacketLength = 268; ///< Statusと最大20個の6軸IMUレコードを収める上限。
        std::atomic<bool> connected{
            false}; ///< Wi-Fi制御権の有効状態。ルーターへの接続状態ではない。アプリはload()で参照する。
        std::atomic<uint32_t> generation{0}; ///< 制御権の取得・解除で更新する世代番号。アプリはload()で参照する。

        /**
         * @brief 表示名、教材用接続キーとNVS名前空間。
         */
        struct Config {
            String name;
            String connectionKey;
            String nvsNamespace = "uroa-net";
        };

        /**
         * @brief 指令・接続通知、BLEの切り替えと省略可能なカメラ処理。
         */
        struct Callbacks {
            std::function<void(const uint8_t *, size_t, uint32_t)> command;
            std::function<void(bool, uint32_t)> session;
            std::function<void()> startBLE;
            std::function<void()> stopBLE;
            std::function<void()> cameraBegin;
            std::function<bool()> cameraAvailable;
            std::function<void(httpd_req_t *)> cameraServe;
        };

        /**
         * @brief 通信開始前のネットワークオブジェクトを構築する。
         */
        WifiNetwork();
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        WifiNetwork(const WifiNetwork &) = delete;
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        WifiNetwork &operator=(const WifiNetwork &) = delete;
        /**
         * @brief NVSのIDとWi-Fi設定を読み、キューと専用タスクを開始する。
         * @param config 教材側から渡す設定。
         * @param callbacks 稼働中保持するコールバック。
         */
        void begin(const Config &config, const Callbacks &callbacks);
        /**
         * @brief 認証済み窓口からの設定JSONを検査し、変更要求を専用タスクへ渡す。
         * @details 必須コールバック: command/session/startBLE/stopBLE。カメラは省略できる。 configure()はBLE等の認証済み窓口だけから呼ぶ。Wi-Fiには公開しない。
         * @param input 認証済み窓口で受信した設定JSON。
         * @return 接続情報、受付結果またはエラーのJSON。
         */
        String configure(const String &input);
        /**
         * @brief BLEの停止予定中または停止済みか調べる。
         * @details BLE停止予定中もtrue。破棄前のCharacteristicに通知しないために使う。
         * @return 破棄前後のBLE Characteristicへ通知してはいけない期間ならtrue。
         */
        bool bleUnavailable() const;
        /**
         * @brief 制御セッションを解除して停止を通知する。機器操作は通知先へ委譲する。
         */
        void stopControl();
        /**
         * @brief 制御ループ側で接続世代の確認とBLEの停止・再開を進める。
         */
        void tick();
        /**
         * @brief Statusをキューへ、HTTP用IMUを最新1件へ保持する。飽和時は制御を解除する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         */
        void publish(const uint8_t *data, size_t length);

        private:
        /**
         * @brief 応答送信が終わるまでOrigin文字列を保持するCORSヘッダー設定。
         */
        class CorsHeaders;
        using Handler = esp_err_t (WifiNetwork::*)(httpd_req_t *);

        /**
         * @brief HTTP要求を登録時の所有元とメンバー関数へ結び付ける。
         */
        struct RouteBinding {
            WifiNetwork *owner;
            Handler handler;
        };

        RouteBinding routes_[16]{};
        size_t routeCount_ = 0;
        Config config_;
        Callbacks callbacks_;
        bool started_ = false;
        uint32_t seen = 0;
        uint32_t announced = 0;
        uint32_t shutdownAt = 0;

        /**
         * @brief 最大長以内の可変長送信内容と接続世代。
         */
        struct Packet {
            uint8_t data[MaxPacketLength];
            uint16_t length;
            uint32_t generation_;
        };

        /**
         * @brief ネットワークタスクへ渡す設定JSONのコピー。
         */
        struct ConfigRequest {
            char text[384];
        };

        QueueHandle_t configQueue = nullptr;
        QueueHandle_t outputQueue = nullptr;
        SemaphoreHandle_t stateLock = nullptr;
        WebSocketsServer sockets{81};
        httpd_handle_t httpServer = nullptr;
        httpd_handle_t cameraServer = nullptr;
        std::atomic<bool> ready{false};
        std::atomic<bool> shutdownBLE{false};
        std::atomic<bool> bleStopped{false};
        String deviceId;
        String hostname;
        String wifiState = "idle";
        String wifiError;
        String pendingSSID;
        String pendingPassword;
        String savedSSID;
        String savedPassword;
        String challenge;
        String controlSession;
        int owner = -1; // -1: HTTP制御、0以上: WebSocket接続番号。
        uint32_t counterHigh = 0;
        uint32_t scanStarted = 0;
        uint32_t wifiStarted = 0;
        uint32_t outputCounter = 0;
        uint64_t counterWindow = 0;
        bool trying = false;
        bool persistPending = false;
        bool scanRunning = false;
        bool services = false;
        uint8_t lastIMU[MaxPacketLength]{};
        size_t lastIMULength = 0;
        bool haveIMU = false;
        String controlOrigin;
        /**
         * @brief 乱数を16進文字列へ変換する。
         * @param bytes 生成する乱数のバイト数。
         * @return 指定バイト数の2倍の長さの文字列。
         */
        String randomHex(size_t bytes);
        /**
         * @brief 教材の接続キーでSHA-256 HMACを計算する。署名文字列の区切りは変更しない。
         * @param message HMAC署名対象の文字列。
         * @return 64文字の16進署名。
         */
        String hmac(const String &message);
        /**
         * @brief 64文字の署名を全桁比較する。
         * @param a 比較する1つ目の署名。
         * @param b 比較する2つ目の署名。
         * @return 両方が64文字で、全桁一致すればtrue。
         */
        bool equalMAC(const String &a, const String &b);
        /**
         * @brief バイナリパケットを16進文字列へ変換する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 入力バイト数の2倍の長さの文字列。
         */
        String packetHex(const uint8_t *data, size_t length);
        /**
         * @brief 16進文字列を指定長のバイナリへ変換する。
         * @param value 検査または変換する値。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 長さと各値を変換できたらtrue。
         */
        bool unhex(const String &value, uint8_t *data, size_t length);
        /**
         * @brief HTTPSまたはlocalhost向けHTTPのOriginか調べる。
         * @param origin 要求元のOrigin。
         * @return 許可するOriginならtrue。
         */
        static bool allowedOrigin(const String &origin);
        /**
         * @brief 機器名、接続状態、接続先をJSONへ書き込む。秘密情報は認証済み窓口に限定する。
         * @param out 出力先バッファーまたはJSONオブジェクト。
         * @param secret 認証済みの応答へ接続キーを含めるか。
         */
        void metadata(JsonObject out, bool secret = false);
        /**
         * @brief 設定操作への情報応答または検索結果のページを組み立てる。
         * @param operation 設定操作名。
         * @param page Wi-Fi検索結果のページ番号。
         * @return 512バイト以内のJSON応答。超過時はエラーJSON。
         */
        String jsonReply(const String &operation, int page = 0);
        /**
         * @brief HTTPヘッダーを最大256バイトまで読み取る。
         * @param req HTTP要求。
         * @param name 取得するヘッダー名。
         * @return ヘッダー値。未指定または長すぎる場合は空文字列。
         */
        static String header(httpd_req_t *req, const char *name);
        /**
         * @brief CORSヘッダーを付けてJSON応答を送る。
         * @param req HTTP要求。
         * @param body 送信するJSON本文。
         * @return HTTP応答送信の結果。
         */
        esp_err_t respond(httpd_req_t *req, const String &body);
        /**
         * @brief 512バイト未満のHTTP本文を最後まで読み取る。
         * @param req HTTP要求。
         * @param text 取得したHTTP本文の出力先。
         * @return 本文を取得できたらtrue。
         */
        bool body(httpd_req_t *req, String &text);
        /**
         * @brief 64件の窓で要求番号の再使用を拒否し、到着順の逆転を許容する。
         * @param value 検査または変換する値。
         * @return 未使用の有効な要求番号ならtrue。
         */
        bool acceptCounter(uint32_t value);
        /**
         * @brief 制御セッション、HMACと要求番号を検査する。
         * @param sid 制御セッション識別子。
         * @param count 署名対象の要求番号。
         * @param path 署名対象の要求パス。
         * @param payload 本文またはWebSocket受信データ。
         * @param mac 受信したHMAC署名。
         * @return 署名と要求番号を受理したらtrue。
         */
        bool authorize(const String &sid, uint32_t count, const String &path, const String &payload, const String &mac);
        /**
         * @brief HTTP制御要求を認証し、失敗時は401を返す。
         * @param req HTTP要求。
         * @param payload 本文またはWebSocket受信データ。
         * @return 要求を認証できたらtrue。
         */
        bool authorizeHTTP(httpd_req_t *req, const String &payload = "");
        /**
         * @brief 認証された要求へ制御権を付与し、接続世代を更新する。
         * @param nonce 制御権取得のchallenge。
         * @param proof 制御権取得の確認用署名。
         * @param origin 要求元のOrigin。
         * @param socketOwner HTTPは−1、WebSocketは接続番号。
         * @return 制御権を取得できたらtrue。
         */
        bool claim(const String &nonce, const String &proof, const String &origin, int socketOwner);
        /**
         * @brief 制御セッションと確認用署名をJSONへまとめる。
         * @return 接続確認用のJSON。
         */
        String claimReply();
        /**
         * @brief CORSのプリフライト要求へ空の応答を返す。
         * @param req HTTP要求。
         * @return HTTP応答送信の結果。
         */
        esp_err_t options(httpd_req_t *req);
        /**
         * @brief 接続情報と制御権取得用challengeを返す。
         * @param req HTTP要求。
         * @return HTTP応答送信の結果。
         */
        esp_err_t info(httpd_req_t *req);
        /**
         * @brief HTTPから制御権を取得し、準備完了を待って接続情報を返す。
         * @param req HTTP要求。
         * @return HTTP応答送信の結果。
         */
        esp_err_t claimHTTP(httpd_req_t *req);
        /**
         * @brief 認証済みHTTP指令を検査して指令コールバックへ渡す。
         * @param req HTTP要求。
         * @return HTTP応答処理の結果。
         */
        esp_err_t commandHTTP(httpd_req_t *req);
        /**
         * @brief 現接続世代のStatusと最新IMUを署名付きJSONで返す。
         * @param req HTTP要求。
         * @return HTTP応答処理の結果。
         */
        esp_err_t eventsHTTP(httpd_req_t *req);
        /**
         * @brief 認証済みHTTP要求で制御権を解除する。
         * @param req HTTP要求。
         * @return HTTP応答処理の結果。
         */
        esp_err_t releaseHTTP(httpd_req_t *req);
        /**
         * @brief 認証済み画像要求をカメラへ渡し、利用不可なら503を返す。
         * @param req HTTP要求。
         * @return HTTP応答処理の結果。
         */
        esp_err_t imageHTTP(httpd_req_t *req);
        /**
         * @brief HTTPルートへ所有元と処理関数を登録する。
         * @param server 登録先のHTTPサーバー。
         * @param uri 登録する要求パス。
         * @param method 登録するHTTPメソッド。
         * @param handler 要求を処理するメンバー関数。
         */
        void registerRoute(httpd_handle_t server, const char *uri, httpd_method_t method, Handler handler);
        /**
         * @brief HTTP、カメラ用HTTP、mDNSとWebSocketを開始する。
         */
        void startServices();
        /**
         * @brief WebSocketの接続、制御権取得、認証済み指令と切断を処理する。
         * @param number WebSocketの接続番号。
         * @param type WebSocketイベント種別。
         * @param payload 本文またはWebSocket受信データ。
         * @param length データ長（バイト）。
         */
        void socketEvent(uint8_t number, WStype_t type, uint8_t *payload, size_t length);
        /**
         * @brief 専用タスクでWi-Fi検索・接続・NVS保存とWebSocket通信を進める。
         */
        void run();
    };
}
