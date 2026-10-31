/**
 * @file uroa_wifi.cpp
 * @brief Wi-Fi設定、認証、HTTP/WebSocket制御と通信タスクを管理する。
 */
#include "uroa_wifi.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <mbedtls/md.h>

namespace uroa {
    /** @brief 通信開始前のネットワークオブジェクトを構築する。 */
    WifiNetwork::WifiNetwork() = default;

    /**
     * @brief 乱数を16進文字列へ変換する。
     * @param bytes 生成する乱数のバイト数。
     * @return 指定バイト数の2倍の長さの文字列。
     */
    String WifiNetwork::randomHex(size_t bytes) {
        const char hex[] = "0123456789abcdef";
        String result;
        result.reserve(bytes * 2);
        for (size_t i = 0; i < bytes; ++i) {
            uint8_t b = esp_random();
            result += hex[b >> 4];
            result += hex[b & 15];
        }
        return result;
    }

    /**
     * @brief 教材の接続キーでSHA-256 HMACを計算する。署名文字列の区切りは変更しない。
     * @details 署名対象の文字列・区切りを変更するとdiagramとの互換性が失われる。
     * @param message HMAC署名対象の文字列。
     * @return 64文字の16進署名。
     */
    String WifiNetwork::hmac(const String &message) {
        uint8_t digest[32];
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
            reinterpret_cast<const uint8_t *>(config_.connectionKey.c_str()), config_.connectionKey.length(),
            reinterpret_cast<const uint8_t *>(message.c_str()), message.length(), digest);
        const char hex[] = "0123456789abcdef";
        String result;
        for (uint8_t b : digest) {
            result += hex[b >> 4];
            result += hex[b & 15];
        }
        return result;
    }

    /**
     * @brief 64文字の署名を全桁比較する。
     * @param a 比較する1つ目の署名。
     * @param b 比較する2つ目の署名。
     * @return 両方が64文字で、全桁一致すればtrue。
     */
    bool WifiNetwork::equalMAC(const String &a, const String &b) {
        if (a.length() != 64 || b.length() != 64) {
            return false;
        }
        uint8_t difference = 0;
        for (size_t i = 0; i < 64; ++i) {
            difference |= a[i] ^ b[i];
        }
        return difference == 0;
    }

    /**
     * @brief バイナリパケットを16進文字列へ変換する。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 入力バイト数の2倍の長さの文字列。
     */
    String WifiNetwork::packetHex(const uint8_t *data, size_t length) {
        const char hex[] = "0123456789abcdef";
        String result;
        for (size_t i = 0; i < length; ++i) {
            result += hex[data[i] >> 4];
            result += hex[data[i] & 15];
        }
        return result;
    }

    /**
     * @brief 16進文字列を指定長のバイナリへ変換する。
     * @param value 検査または変換する値。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さと各値を変換できたらtrue。
     */
    bool WifiNetwork::unhex(const String &value, uint8_t *data, size_t length) {
        if (value.length() != length * 2) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            char pair[] = {value[i * 2], value[i * 2 + 1], 0};
            char *end;
            unsigned long v = strtoul(pair, &end, 16);
            if (*end) {
                return false;
            }
            data[i] = v;
        }
        return true;
    }

    /**
     * @brief HTTPSまたはlocalhost向けHTTPのOriginか調べる。
     * @param origin 要求元のOrigin。
     * @return 許可するOriginならtrue。
     */
    bool WifiNetwork::allowedOrigin(const String &origin) {
        return origin.startsWith("https://") || origin.startsWith("http://localhost:") ||
            origin.startsWith("http://127.0.0.1:");
    }

    /**
     * @brief 機器名、接続状態、接続先をJSONへ書き込む。秘密情報は認証済み窓口に限定する。
     * @details HTTP/BLEで同じ接続情報を返す。キーを含むinfoはBLE認証後だけに使う。
     * @param out 出力先バッファーまたはJSONオブジェクト。
     * @param secret 認証済みの応答へ接続キーを含めるか。
     */
    void WifiNetwork::metadata(JsonObject out, bool secret) {
        out["id"] = deviceId;
        out["name"] = config_.name;
        out["host"] = hostname + ".local";
        out["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
        out["port"] = 80;
        out["wsPort"] = 81;
        out["cameraPort"] = 82;
        out["camera"] = (callbacks_.cameraAvailable && callbacks_.cameraAvailable());
        out["state"] = wifiState;
        out["error"] = wifiError;
        if (secret) {
            out["key"] = config_.connectionKey;
        }
    }

    /**
     * @brief 設定操作への情報応答または検索結果のページを組み立てる。
     * @param operation 設定操作名。
     * @param page Wi-Fi検索結果のページ番号。
     * @return 512バイト以内のJSON応答。超過時はエラーJSON。
     */
    String WifiNetwork::jsonReply(const String &operation, int page) {
        StaticJsonDocument<768> doc;
        if (operation != "list") {
            metadata(doc.to<JsonObject>(), operation == "info");
        }
        if (operation == "list") {
            auto list = doc.createNestedArray("networks");
            int count = min(int(WiFi.scanComplete()), 128);
            doc["pages"] = count > 0 ? (count + 1) / 2 : 0;
            for (int i = page * 2; i < count && i < page * 2 + 2; ++i) {
                auto item = list.createNestedObject();
                item["ssid"] = WiFi.SSID(i);
                item["rssi"] = WiFi.RSSI(i);
                item["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
            }
        }
        String result;
        serializeJson(doc, result);
        if (result.length() > 512) {
            return "{\"error\":\"Response too large\"}";
        }
        return result;
    }

    /**
     * @brief HTTPヘッダーを最大256バイトまで読み取る。
     * @param req HTTP要求。
     * @param name 取得するヘッダー名。
     * @return ヘッダー値。未指定または長すぎる場合は空文字列。
     */
    String WifiNetwork::header(httpd_req_t *req, const char *name) {
        size_t length = httpd_req_get_hdr_value_len(req, name);
        if (!length || length > 256) {
            return "";
        }
        char buffer[257];
        httpd_req_get_hdr_value_str(req, name, buffer, sizeof buffer);
        return String(buffer);
    }

    /**
     * @brief 応答送信が終わるまでOrigin文字列を保持するCORSヘッダー設定。
     * @details ESP-IDFはヘッダー文字列への参照を保持する。応答送信が終わるまでOriginを保持する。
     */
    class WifiNetwork::CorsHeaders {
        const String origin;

        public:
        /**
         * @brief 応答に必要なCORSヘッダーを設定し、Originの寿命を確保する。
         * @param req HTTP要求。
         */
        explicit CorsHeaders(httpd_req_t *req) : origin(header(req, "Origin")) {
            if (allowedOrigin(origin)) {
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", origin.c_str());
            }
            httpd_resp_set_hdr(req, "Vary", "Origin");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            httpd_resp_set_hdr(
                req, "Access-Control-Allow-Headers", "Content-Type, X-Uroa-Session, X-Uroa-Counter, X-Uroa-Mac");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Private-Network", "true");
            httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        }
    };

    /**
     * @brief CORSヘッダーを付けてJSON応答を送る。
     * @param req HTTP要求。
     * @param body 送信するJSON本文。
     * @return HTTP応答送信の結果。
     */
    esp_err_t WifiNetwork::respond(httpd_req_t *req, const String &body) {
        const CorsHeaders headers(req);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, body.c_str(), body.length());
    }

    /**
     * @brief 512バイト未満のHTTP本文を最後まで読み取る。
     * @param req HTTP要求。
     * @param text 取得したHTTP本文の出力先。
     * @return 本文を取得できたらtrue。
     */
    bool WifiNetwork::body(httpd_req_t *req, String &text) {
        if (req->content_len <= 0 || req->content_len >= 512) {
            return false;
        }
        char buffer[512];
        int received = 0;
        while (received < req->content_len) {
            int n = httpd_req_recv(req, buffer + received, req->content_len - received);
            if (n <= 0) {
                return false;
            }
            received += n;
        }
        buffer[received] = 0;
        text = buffer;
        return true;
    }

    /**
     * @brief 64件の窓で要求番号の再使用を拒否し、到着順の逆転を許容する。
     * @details 64件の窓で要求番号の再利用を拒否し、並列HTTPの到着順の違いを許容する。
     * @param value 検査または変換する値。
     * @return 未使用の有効な要求番号ならtrue。
     */
    bool WifiNetwork::acceptCounter(uint32_t value) {
        if (!value) {
            return false;
        }
        if (value > counterHigh) {
            uint32_t shift = value - counterHigh;
            counterWindow = shift >= 64 ? 1 : (counterWindow << shift) | 1;
            counterHigh = value;
            return true;
        }
        uint32_t offset = counterHigh - value;
        if (offset >= 64 || (counterWindow & (uint64_t(1) << offset))) {
            return false;
        }
        counterWindow |= uint64_t(1) << offset;
        return true;
    }

    /**
     * @brief 制御セッション、HMACと要求番号を検査する。
     * @param sid 制御セッション識別子。
     * @param count 署名対象の要求番号。
     * @param path 署名対象の要求パス。
     * @param payload 本文またはWebSocket受信データ。
     * @param mac 受信したHMAC署名。
     * @return 署名と要求番号を受理したらtrue。
     */
    bool WifiNetwork::authorize(
        const String &sid, uint32_t count, const String &path, const String &payload, const String &mac) {
        if (!connected || !ready || sid != controlSession) {
            return false;
        }
        if (!equalMAC(hmac(sid + "|" + String(count) + "|" + path + "|" + payload), mac)) {
            return false;
        }
        return acceptCounter(count);
    }

    /**
     * @brief HTTP制御要求を認証し、失敗時は401を返す。
     * @param req HTTP要求。
     * @param payload 本文またはWebSocket受信データ。
     * @return 要求を認証できたらtrue。
     */
    bool WifiNetwork::authorizeHTTP(httpd_req_t *req, const String &payload) {
        xSemaphoreTake(stateLock, portMAX_DELAY);
        bool ok = authorize(header(req, "X-Uroa-Session"), header(req, "X-Uroa-Counter").toInt(), req->uri, payload,
            header(req, "X-Uroa-Mac"));
        xSemaphoreGive(stateLock);
        if (!ok) {
            const CorsHeaders headers(req);
            httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
        }
        return ok;
    }

    /**
     * @brief 認証された要求へ制御権を付与し、接続世代を更新する。
     * @details 制御権の取得は1セッションだけ。コールバックはI2Cを操作せず停止を予約する。
     * @param nonce 制御権取得のchallenge。
     * @param proof 制御権取得の確認用署名。
     * @param origin 要求元のOrigin。
     * @param socketOwner HTTPは−1、WebSocketは接続番号。
     * @return 制御権を取得できたらtrue。
     */
    bool WifiNetwork::claim(const String &nonce, const String &proof, const String &origin, int socketOwner) {
        if (connected || !allowedOrigin(origin) || nonce != challenge || nonce.isEmpty() ||
            !equalMAC(hmac("claim|" + deviceId + "|" + nonce + "|" + origin), proof)) {
            return false;
        }
        controlSession = randomHex(16);
        counterHigh = 0;
        counterWindow = 0;
        controlOrigin = origin;
        owner = socketOwner;
        ready = false;
        haveIMU = false;
        outputCounter = 0;
        ++generation;
        connected = true;
        callbacks_.session(true, generation);
        return true;
    }

    /**
     * @brief 制御セッションと確認用署名をJSONへまとめる。
     * @return 接続確認用のJSON。
     */
    String WifiNetwork::claimReply() {
        StaticJsonDocument<768> doc;
        metadata(doc.to<JsonObject>());
        doc["session"] = controlSession;
        doc["proof"] = hmac("ready|" + deviceId + "|" + challenge + "|" + controlSession + "|" + hostname + ".local|" +
            WiFi.localIP().toString());
        String result;
        serializeJson(doc, result);
        return result;
    }

    /**
     * @brief CORSのプリフライト要求へ空の応答を返す。
     * @param req HTTP要求。
     * @return HTTP応答送信の結果。
     */
    esp_err_t WifiNetwork::options(httpd_req_t *req) {
        const CorsHeaders headers(req);
        return httpd_resp_send(req, "", 0);
    }

    /**
     * @brief 接続情報と制御権取得用challengeを返す。
     * @param req HTTP要求。
     * @return HTTP応答送信の結果。
     */
    esp_err_t WifiNetwork::info(httpd_req_t *req) {
        xSemaphoreTake(stateLock, portMAX_DELAY);
        if (!connected) {
            challenge = randomHex(16);
        }
        StaticJsonDocument<768> doc;
        metadata(doc.to<JsonObject>());
        doc["challenge"] = challenge;
        String result;
        serializeJson(doc, result);
        xSemaphoreGive(stateLock);
        return respond(req, result);
    }

    /**
     * @brief HTTPから制御権を取得し、準備完了を待って接続情報を返す。
     * @param req HTTP要求。
     * @return HTTP応答送信の結果。
     */
    esp_err_t WifiNetwork::claimHTTP(httpd_req_t *req) {
        String input;
        StaticJsonDocument<512> doc;
        if (!body(req, input) || deserializeJson(doc, input)) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        }
        xSemaphoreTake(stateLock, portMAX_DELAY);
        bool ok = claim(doc["challenge"] | "", doc["proof"] | "", header(req, "Origin"), -1);
        xSemaphoreGive(stateLock);
        if (!ok) {
            const CorsHeaders headers(req);
            return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Connection refused");
        }
        for (int i = 0; i < 100 && connected && !ready; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (!ready) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_send(req, "Not ready", HTTPD_RESP_USE_STRLEN);
        }
        xSemaphoreTake(stateLock, portMAX_DELAY);
        String reply = claimReply();
        xSemaphoreGive(stateLock);
        shutdownBLE = true;
        return respond(req, reply);
    }

    /**
     * @brief 認証済みHTTP指令を検査して指令コールバックへ渡す。
     * @param req HTTP要求。
     * @return HTTP応答処理の結果。
     */
    esp_err_t WifiNetwork::commandHTTP(httpd_req_t *req) {
        String payload;
        if (!body(req, payload)) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        }
        if (!authorizeHTTP(req, payload)) {
            return ESP_OK;
        }
        uint8_t data[10];
        if (!unhex(payload, data, sizeof data)) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid command");
        }
        callbacks_.command(data, sizeof data, generation);
        return respond(req, "{\"accepted\":true}");
    }

    /**
     * @brief 現接続世代のStatusと最新IMUを署名付きJSONで返す。
     * @param req HTTP要求。
     * @return HTTP応答処理の結果。
     */
    esp_err_t WifiNetwork::eventsHTTP(httpd_req_t *req) {
        if (!authorizeHTTP(req)) {
            return ESP_OK;
        }
        StaticJsonDocument<2048> doc;
        auto packets = doc.createNestedArray("packets");
        Packet p;
        for (int n = 0; n < 16 && xQueueReceive(outputQueue, &p, 0) == pdTRUE; ++n) {
            if (p.generation_ == generation) {
                packets.add(packetHex(p.data, p.length));
            }
        }
        xSemaphoreTake(stateLock, portMAX_DELAY);
        if (haveIMU) {
            packets.add(packetHex(lastIMU, lastIMULength));
            haveIMU = false;
        }
        doc["session"] = controlSession;
        String data;
        serializeJson(packets, data);
        String requestCount = header(req, "X-Uroa-Counter");
        doc["counter"] = requestCount;
        doc["proof"] = hmac(controlSession + "|events|" + requestCount + "|" + data);
        xSemaphoreGive(stateLock);
        String result;
        serializeJson(doc, result);
        return respond(req, result);
    }

    /**
     * @brief 認証済みHTTP要求で制御権を解除する。
     * @param req HTTP要求。
     * @return HTTP応答処理の結果。
     */
    esp_err_t WifiNetwork::releaseHTTP(httpd_req_t *req) {
        if (!authorizeHTTP(req)) {
            return ESP_OK;
        }
        stopControl();
        return respond(req, "{\"released\":true}");
    }

    /**
     * @brief 認証済み画像要求をカメラへ渡し、利用不可なら503を返す。
     * @param req HTTP要求。
     * @return HTTP応答処理の結果。
     */
    esp_err_t WifiNetwork::imageHTTP(httpd_req_t *req) {
        if (!authorizeHTTP(req)) {
            return ESP_OK;
        }
        const CorsHeaders headers(req);
        if (callbacks_.cameraServe) {
            callbacks_.cameraServe(req);
        }
        else {
            // カメラ未接続の教材でもHTTP要求を完了させる。
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_send(req, "Camera unavailable", HTTPD_RESP_USE_STRLEN);
        }
        return ESP_OK;
    }

    /**
     * @brief HTTPルートへ所有元と処理関数を登録する。
     * @details 登録先を要求ごとに保持し、グローバルな現在インスタンスを使わない。
     * @param server 登録先のHTTPサーバー。
     * @param uri 登録する要求パス。
     * @param method 登録するHTTPメソッド。
     * @param handler 要求を処理するメンバー関数。
     */
    void WifiNetwork::registerRoute(httpd_handle_t server, const char *uri, httpd_method_t method, Handler handler) {
        auto &binding = routes_[routeCount_++];
        binding.owner = this;
        binding.handler = handler;
        httpd_uri_t route{};
        route.uri = uri;
        route.method = method;
        route.user_ctx = &binding;
        route.handler = [](httpd_req_t *request) -> esp_err_t {
            auto &target = *static_cast<RouteBinding *>(request->user_ctx);
            return (target.owner->*target.handler)(request);
        };
        httpd_register_uri_handler(server, &route);
    }

    /**
     * @brief HTTP、カメラ用HTTP、mDNSとWebSocketを開始する。
     */
    void WifiNetwork::startServices() {
        if (services) {
            return;
        }
        if (callbacks_.cameraBegin) {
            callbacks_.cameraBegin();
        }
        MDNS.begin(hostname.c_str());
        MDNS.addService("http", "tcp", 80);
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = 80;
        config.ctrl_port = 32768;
        config.stack_size = 8192;
        config.max_uri_handlers = 16;
        config.recv_wait_timeout = 2;
        config.send_wait_timeout = 2;
        if (httpd_start(&httpServer, &config) != ESP_OK) {
            wifiError = "HTTP start failed";
            return;
        }
        registerRoute(httpServer, "/info", HTTP_GET, &WifiNetwork::info);
        registerRoute(httpServer, "/claim", HTTP_POST, &WifiNetwork::claimHTTP);
        registerRoute(httpServer, "/command", HTTP_POST, &WifiNetwork::commandHTTP);
        registerRoute(httpServer, "/events", HTTP_GET, &WifiNetwork::eventsHTTP);
        registerRoute(httpServer, "/release", HTTP_POST, &WifiNetwork::releaseHTTP);
        for (const char *path : {"/info", "/claim", "/command", "/events", "/release"}) {
            registerRoute(httpServer, path, HTTP_OPTIONS, &WifiNetwork::options);
        }
        config.server_port = 82;
        config.ctrl_port = 32769;
        config.max_uri_handlers = 4;
        if (httpd_start(&cameraServer, &config) == ESP_OK) {
            registerRoute(cameraServer, "/capture", HTTP_GET, &WifiNetwork::imageHTTP);
            registerRoute(cameraServer, "/capture", HTTP_OPTIONS, &WifiNetwork::options);
        }
        sockets.begin();
        services = true;
    }

    /**
     * @brief WebSocketの接続、制御権取得、認証済み指令と切断を処理する。
     * @param number WebSocketの接続番号。
     * @param type WebSocketイベント種別。
     * @param payload 本文またはWebSocket受信データ。
     * @param length データ長（バイト）。
     */
    void WifiNetwork::socketEvent(uint8_t number, WStype_t type, uint8_t *payload, size_t length) {
        if (type == WStype_DISCONNECTED) {
            if (connected && owner == number) {
                stopControl();
            }
            return;
        }
        if (type == WStype_CONNECTED) {
            xSemaphoreTake(stateLock, portMAX_DELAY);
            StaticJsonDocument<768> doc;
            metadata(doc.to<JsonObject>());
            if (!connected) {
                challenge = randomHex(16);
            }
            doc["challenge"] = challenge;
            String result;
            serializeJson(doc, result);
            xSemaphoreGive(stateLock);
            sockets.sendTXT(number, result);
            return;
        }
        if (type != WStype_TEXT || length >= 512) {
            return;
        }
        StaticJsonDocument<768> doc;
        if (deserializeJson(doc, payload, length)) {
            return;
        }
        String op = doc["op"] | "";
        if (op == "claim") {
            xSemaphoreTake(stateLock, portMAX_DELAY);
            bool ok = claim(doc["challenge"] | "", doc["proof"] | "", doc["origin"] | "", number);
            xSemaphoreGive(stateLock);
            if (!ok) {
                sockets.disconnect(number);
            }
            return;
        }
        if (!connected || !ready || owner != number) {
            return;
        }
        String data = doc["data"] | "";
        xSemaphoreTake(stateLock, portMAX_DELAY);
        bool ok = authorize(doc["session"] | "", doc["counter"] | uint32_t(0), "/command", data, doc["mac"] | "");
        xSemaphoreGive(stateLock);
        uint8_t bytes[10];
        if (ok && unhex(data, bytes, sizeof bytes)) {
            callbacks_.command(bytes, 10, generation);
        }
        else {
            sockets.disconnect(number);
        }
    }

    /**
     * @brief 専用タスクでWi-Fi検索・接続・NVS保存とWebSocket通信を進める。
     * @details 専用タスクでWi-Fi検索・接続・保存とWebSocketを進める。制御ループを待たせない。
     */
    void WifiNetwork::run() {
        sockets.onEvent([this](uint8_t number, WStype_t type, uint8_t *payload, size_t length) {
            socketEvent(number, type, payload, length);
        });
        uint32_t lastSocketReady = 0;
        for (;;) {
            ConfigRequest req;
            if (xQueueReceive(configQueue, &req, 0) == pdTRUE) {
                StaticJsonDocument<512> doc;
                if (!deserializeJson(doc, req.text)) {
                    String op = doc["op"] | "";
                    xSemaphoreTake(stateLock, portMAX_DELAY);
                    if (!connected) {
                        wifiError = "";
                        if (op == "scan" && !trying && !scanRunning) {
                            WiFi.scanDelete();
                            WiFi.scanNetworks(true, true);
                            scanRunning = true;
                            wifiState = "scanning";
                            scanStarted = millis();
                        }
                        else if (op == "configure" && !scanRunning) {
                            String ssid = doc["ssid"] | "";
                            String password = doc["password"] | "";
                            if (!ssid.length() || ssid.length() > 32 || (password.length() && password.length() < 8) ||
                                password.length() > 63) {
                                wifiError = "Invalid Wi-Fi settings";
                            }
                            else {
                                stopControl();
                                WiFi.disconnect(false, false);
                                WiFi.begin(ssid.c_str(), password.c_str());
                                pendingSSID = ssid;
                                pendingPassword = password;
                                trying = persistPending = true;
                                wifiStarted = millis();
                                wifiState = "connecting";
                            }
                            password = "";
                        }
                        else if (op == "forget") {
                            Preferences prefs;
                            if (prefs.begin(config_.nvsNamespace.c_str(), false)) {
                                prefs.remove("wifi");
                                prefs.end();
                            }
                            savedSSID = savedPassword = pendingSSID = pendingPassword = "";
                            WiFi.disconnect(false, true);
                            trying = persistPending = false;
                            wifiState = "idle";
                        }
                    }
                    else {
                        wifiError = "Disconnect Wi-Fi control before setup";
                    }
                    xSemaphoreGive(stateLock);
                }
                memset(&req, 0, sizeof req);
            }
            if (scanRunning) {
                int result = WiFi.scanComplete();
                if (result != WIFI_SCAN_RUNNING || millis() - scanStarted > 15000) {
                    xSemaphoreTake(stateLock, portMAX_DELAY);
                    scanRunning = false;
                    wifiState = WiFi.status() == WL_CONNECTED ? "connected" : "idle";
                    if (result < 0) {
                        wifiError = "Wi-Fi scan failed";
                    }
                    xSemaphoreGive(stateLock);
                }
            }
            if (trying && WiFi.status() == WL_CONNECTED) {
                startServices();
                xSemaphoreTake(stateLock, portMAX_DELAY);
                wifiState = services ? "connected" : "error";
                if (persistPending && services) {
                    // SSIDとパスワードを1つのJSONにし、NVSの値をまとめて置き換える。
                    StaticJsonDocument<192> doc;
                    doc["ssid"] = pendingSSID;
                    doc["password"] = pendingPassword;
                    String value;
                    serializeJson(doc, value);
                    Preferences prefs;
                    bool stored =
                        prefs.begin(config_.nvsNamespace.c_str(), false) && prefs.putString("wifi", value) > 0;
                    prefs.end();
                    if (stored) {
                        savedSSID = pendingSSID;
                        savedPassword = pendingPassword;
                    }
                    else {
                        wifiError = "Wi-Fi connected; NVS save failed";
                    }
                    value = "";
                }
                trying = persistPending = false;
                pendingPassword = "";
                xSemaphoreGive(stateLock);
            }
            if (trying && millis() - wifiStarted > 20000) {
                xSemaphoreTake(stateLock, portMAX_DELAY);
                trying = persistPending = false;
                pendingPassword = "";
                wifiState = "error";
                wifiError = "Wi-Fi connection failed; check SSID/password";
                xSemaphoreGive(stateLock);
            }
            if (services && WiFi.status() != WL_CONNECTED && !trying && !scanRunning) {
                stopControl();
                xSemaphoreTake(stateLock, portMAX_DELAY);
                wifiState = "error";
                wifiError = "Wi-Fi disconnected";
                xSemaphoreGive(stateLock);
            }
            if (services) {
                sockets.loop();
            }
            if (connected && ready && owner >= 0) {
                if (lastSocketReady != generation) {
                    xSemaphoreTake(stateLock, portMAX_DELAY);
                    String reply = claimReply();
                    xSemaphoreGive(stateLock);
                    sockets.sendTXT(owner, reply);
                    lastSocketReady = generation;
                    shutdownBLE = true;
                }
                Packet p;
                for (int n = 0; n < 16 && xQueueReceive(outputQueue, &p, 0) == pdTRUE; ++n) {
                    if (p.generation_ == generation) {
                        xSemaphoreTake(stateLock, portMAX_DELAY);
                        StaticJsonDocument<2 * MaxPacketLength + 256> doc;
                        String data = packetHex(p.data, p.length);
                        uint32_t count = ++outputCounter;
                        doc["data"] = data;
                        doc["counter"] = count;
                        doc["proof"] = hmac(controlSession + "|packet|" + String(count) + "|" + data);
                        String result;
                        serializeJson(doc, result);
                        xSemaphoreGive(stateLock);
                        sockets.sendTXT(owner, result);
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    /**
     * @brief NVSのIDとWi-Fi設定を読み、キューと専用タスクを開始する。
     * @details NVS名と表示名は教材側から受け取る。既存ID・Wi-Fi設定の形式を維持する。
     * @param config 教材側から渡す設定。
     * @param callbacks 稼働中保持するコールバック。
     */
    void WifiNetwork::begin(const Config &config, const Callbacks &callbacks) {
        if (started_) {
            return;
        }
        config_ = config;
        callbacks_ = callbacks;
        started_ = true;
        stateLock = xSemaphoreCreateMutex();
        configQueue = xQueueCreate(4, sizeof(ConfigRequest));
        outputQueue = xQueueCreate(32, sizeof(Packet));
        if (!stateLock || !configQueue || !outputQueue) {
            Serial.println("Network allocation failed");
            while (true) {
                delay(1000);
            }
        }
        Preferences prefs;
        if (!prefs.begin(config_.nvsNamespace.c_str(), false)) {
            Serial.println("Network NVS unavailable");
            while (true) {
                delay(1000);
            }
        }
        deviceId = prefs.getString("id", "");
        if (deviceId.isEmpty()) {
            deviceId = "uroa-" + randomHex(6);
            prefs.putString("id", deviceId);
        }
        prefs.remove("key"); // 旧個別キーだけを削除し、Wi-Fi設定とIDは維持する。
        hostname = deviceId;
        StaticJsonDocument<192> doc;
        String savedConfig = prefs.getString("wifi", "");
        if (!deserializeJson(doc, savedConfig)) {
            savedSSID = doc["ssid"] | "";
            savedPassword = doc["password"] | "";
        }
        savedConfig = "";
        prefs.end();
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        if (!savedSSID.isEmpty()) {
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            trying = true;
            wifiStarted = millis();
            wifiState = "connecting";
        }
        xTaskCreate(
            [](void *context) {
                static_cast<WifiNetwork *>(context)->run();
            },
            "uroa-network", 12288, this, 1, nullptr);
    }

    /**
     * @brief BLEの停止予定中または停止済みか調べる。
     * @return 破棄前後のBLE Characteristicへ通知してはいけない期間ならtrue。
     */
    bool WifiNetwork::bleUnavailable() const {
        return shutdownBLE || bleStopped;
    }

    /**
     * @brief 制御セッションを解除して停止を通知する。機器操作は通知先へ委譲する。
     * @details どの通信タスクからも呼べる。機器の停止はsessionコールバック先に委譲する。
     */
    void WifiNetwork::stopControl() {
        if (connected.exchange(false)) {
            ready = false;
            ++generation;
            callbacks_.session(false, generation);
        }
    }

    /**
     * @brief 制御ループ側で接続世代の確認とBLEの停止・再開を進める。
     * @details 制御ループが接続世代変更を処理する時間を確保してからreadyを公開する。
     */
    void WifiNetwork::tick() {
        if (connected && !ready) {
            if (seen == generation) {
                ready = true;
            }
            else {
                seen = generation;
            }
        }
        if (shutdownBLE && !bleStopped) {
            if (!shutdownAt) {
                shutdownAt = millis();
            }
            if (millis() - shutdownAt > 500) {
                callbacks_.stopBLE();
                bleStopped = true;
                shutdownBLE = false;
                shutdownAt = 0;
            }
        }
        if (!connected && bleStopped) {
            shutdownBLE = false;
            callbacks_.startBLE();
            bleStopped = false;
        }
        if (connected) {
            announced = generation;
        }
        else if (announced) {
            announced = 0;
            shutdownBLE = false;
            shutdownAt = 0;
        }
    }

    /**
     * @brief Statusをキューへ、HTTP用IMUを最新1件へ保持する。飽和時は制御を解除する。
     * @details Statusはキュー、HTTPのIMUは最新1件だけを保持して古い計測の滞留を防ぐ。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     */
    void WifiNetwork::publish(const uint8_t *data, size_t length) {
        if (!connected || !ready || length == 0 || length > MaxPacketLength) {
            return;
        }
        if (length != 12) {
            xSemaphoreTake(stateLock, portMAX_DELAY);
            memcpy(lastIMU, data, length);
            lastIMULength = length;
            haveIMU = true;
            xSemaphoreGive(stateLock);
            if (owner < 0) {
                return;
            }
        }
        Packet packet{};
        memcpy(packet.data, data, length);
        packet.length = length;
        packet.generation_ = generation;
        if (xQueueSend(outputQueue, &packet, 0) != pdTRUE) {
            stopControl();
        }
    }

    /**
     * @brief 認証済み窓口からの設定JSONを検査し、変更要求を専用タスクへ渡す。
     * @details 認証済みの設定窓口から呼ぶJSON操作。変更要求はネットワークタスクへ送る。
     * @param input 認証済み窓口で受信した設定JSON。
     * @return 接続情報、受付結果またはエラーのJSON。
     */
    String WifiNetwork::configure(const String &input) {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, input)) {
            return "{\"error\":\"Invalid request\"}";
        }
        String op = doc["op"] | "";
        if (op == "info" || op == "list") {
            xSemaphoreTake(stateLock, portMAX_DELAY);
            String reply = jsonReply(op, constrain(doc["page"] | 0, 0, 64));
            xSemaphoreGive(stateLock);
            return reply;
        }
        if (op == "scan" || op == "configure" || op == "forget") {
            ConfigRequest request{};
            input.toCharArray(request.text, sizeof request.text);
            return xQueueSend(configQueue, &request, 0) == pdTRUE ? "{\"accepted\":true}" : "{\"error\":\"Busy\"}";
        }
        return "{\"error\":\"Unknown operation\"}";
    }

}
