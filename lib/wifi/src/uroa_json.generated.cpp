/**
 * @file uroa_json.generated.cpp
 * @brief JSONの読み書きと既存Wi-Fi窓口への配送。手動編集しない。
 */
#include "uroa_json.generated.h"
#include "uroa_wifi.h"
#include <memory>
#include <new>
namespace uroa {
    namespace jsonwire {
        EndpointFields readEndpointFields(JsonVariantConst input) {
            EndpointFields value{};
            value.id = input["id"] | "";
            value.name = input["name"] | "";
            value.host = input["host"] | "";
            value.ip = input["ip"] | "";
            value.port = input["port"] | uint32_t(0);
            value.wsPort = input["wsPort"] | uint32_t(0);
            value.cameraPort = input["cameraPort"] | uint32_t(0);
            value.camera = input["camera"] | false;
            return value;
        }
        void writeEndpointFields(JsonObject out, const String &id, const String &name, const String &host, const String &ip, uint32_t port, uint32_t wsPort, uint32_t cameraPort, bool camera) {
            out["id"] = id;
            out["name"] = name;
            out["host"] = host;
            out["ip"] = ip;
            out["port"] = port;
            out["wsPort"] = wsPort;
            out["cameraPort"] = cameraPort;
            out["camera"] = camera;
        }
        ProvisionStatus readProvisionStatus(JsonVariantConst input) {
            ProvisionStatus value{};
            value.state = input["state"] | "";
            value.error = input["error"] | "";
            return value;
        }
        void writeProvisionStatus(JsonObject out, const String &state, const String &error) {
            out["state"] = state;
            out["error"] = error;
        }
        SecretField readSecretField(JsonVariantConst input) {
            SecretField value{};
            value.key = input["key"] | "";
            return value;
        }
        void writeSecretField(JsonObject out, const String &key) {
            out["key"] = key;
        }
        ProvisionQuery readProvisionQuery(JsonVariantConst input) {
            ProvisionQuery value{};
            value.op = input["op"] | "";
            value.page = input["page"] | int32_t(0);
            return value;
        }
        void writeProvisionQuery(JsonObject out, const String &op, int32_t page) {
            out["op"] = op;
            out["page"] = page;
        }
        ProvisionList readProvisionList(JsonVariantConst input) {
            ProvisionList value{};
            value.op = input["op"] | "";
            value.page = input["page"] | uint32_t(0);
            return value;
        }
        void writeProvisionList(JsonObject out, uint32_t page) {
            out["op"] = "list";
            out["page"] = page;
        }
        ProvisionConfigure readProvisionConfigure(JsonVariantConst input) {
            ProvisionConfigure value{};
            value.op = input["op"] | "";
            value.ssid = input["ssid"] | "";
            value.password = input["password"] | "";
            return value;
        }
        void writeProvisionConfigure(JsonObject out, const String &ssid, const String &password) {
            out["op"] = "configure";
            out["ssid"] = ssid;
            out["password"] = password;
        }
        WifiNetworkRow readWifiNetworkRow(JsonVariantConst input) {
            WifiNetworkRow value{};
            value.ssid = input["ssid"] | "";
            value.rssi = input["rssi"] | int32_t(0);
            value.open = input["open"] | false;
            return value;
        }
        void writeWifiNetworkRow(JsonObject out, const String &ssid, int32_t rssi, bool open) {
            out["ssid"] = ssid;
            out["rssi"] = rssi;
            out["open"] = open;
        }
        HttpClaim readHttpClaim(JsonVariantConst input) {
            HttpClaim value{};
            value.challenge = input["challenge"] | "";
            value.proof = input["proof"] | "";
            return value;
        }
        void writeHttpClaim(JsonObject out, const String &challenge, const String &proof) {
            out["challenge"] = challenge;
            out["proof"] = proof;
        }
        SocketClaim readSocketClaim(JsonVariantConst input) {
            SocketClaim value{};
            value.op = input["op"] | "";
            value.challenge = input["challenge"] | "";
            value.proof = input["proof"] | "";
            value.origin = input["origin"] | "";
            return value;
        }
        void writeSocketClaim(JsonObject out, const String &challenge, const String &proof, const String &origin) {
            out["op"] = "claim";
            out["challenge"] = challenge;
            out["proof"] = proof;
            out["origin"] = origin;
        }
        ReadyFields readReadyFields(JsonVariantConst input) {
            ReadyFields value{};
            value.session = input["session"] | "";
            value.proof = input["proof"] | "";
            return value;
        }
        void writeReadyFields(JsonObject out, const String &session, const String &proof) {
            out["session"] = session;
            out["proof"] = proof;
        }
        SocketCommand readSocketCommand(JsonVariantConst input) {
            SocketCommand value{};
            value.op = input["op"] | "";
            value.session = input["session"] | "";
            value.counter = input["counter"] | uint32_t(0);
            value.data = input["data"] | "";
            value.mac = input["mac"] | "";
            return value;
        }
        void writeSocketCommand(JsonObject out, const String &op, const String &session, uint32_t counter, const String &data, const String &mac) {
            out["op"] = op;
            out["session"] = session;
            out["counter"] = counter;
            out["data"] = data;
            out["mac"] = mac;
        }
        ReadReply readReadReply(JsonVariantConst input) {
            ReadReply value{};
            value.session = input["session"] | "";
            value.counter = input["counter"] | "";
            value.data = input["data"] | "";
            value.proof = input["proof"] | "";
            return value;
        }
        void writeReadReply(JsonObject out, const String &session, const String &counter, const String &data, const String &proof) {
            out["session"] = session;
            out["counter"] = counter;
            out["data"] = data;
            out["proof"] = proof;
        }
        SignedFields readSignedFields(JsonVariantConst input) {
            SignedFields value{};
            value.session = input["session"] | "";
            value.counter = input["counter"] | "";
            value.proof = input["proof"] | "";
            return value;
        }
        void writeSignedFields(JsonObject out, const String &session, const String &counter, const String &proof) {
            out["session"] = session;
            out["counter"] = counter;
            out["proof"] = proof;
        }
        SocketPacket readSocketPacket(JsonVariantConst input) {
            SocketPacket value{};
            value.data = input["data"] | "";
            value.counter = input["counter"] | uint32_t(0);
            value.proof = input["proof"] | "";
            return value;
        }
        void writeSocketPacket(JsonObject out, const String &data, uint32_t counter, const String &proof) {
            out["data"] = data;
            out["counter"] = counter;
            out["proof"] = proof;
        }
        EventsReply readEventsReply(JsonVariantConst input) {
            EventsReply value{};
            value.packets = input["packets"].as<JsonArrayConst>();
            value.session = input["session"] | "";
            value.counter = input["counter"] | "";
            value.proof = input["proof"] | "";
            return value;
        }
        void writeEventsReply(JsonObject out, const String &packets, const String &session, const String &counter, const String &proof) {
            out["packets"] = packets;
            out["session"] = session;
            out["counter"] = counter;
            out["proof"] = proof;
        }
        const char AcceptedJson[] = "{\"accepted\":true}";
        Accepted readAccepted(JsonVariantConst input) {
            Accepted value{};
            value.accepted = input["accepted"] | false;
            return value;
        }
        void writeAccepted(JsonObject out) {
            out["accepted"] = true;
        }
        RawBytes readRawBytes(JsonVariantConst input) {
            RawBytes value{};
            value.data = input["data"] | "";
            return value;
        }
        void writeRawBytes(JsonObject out, const String &data) {
            out["data"] = data;
        }
        ChallengeFields readChallengeFields(JsonVariantConst input) {
            ChallengeFields value{};
            value.challenge = input["challenge"] | "";
            return value;
        }
        void writeChallengeFields(JsonObject out, const String &challenge) {
            out["challenge"] = challenge;
        }
        ListPaging readListPaging(JsonVariantConst input) {
            ListPaging value{};
            value.pages = input["pages"] | uint32_t(0);
            return value;
        }
        void writeListPaging(JsonObject out, uint32_t pages) {
            out["pages"] = pages;
        }
        ProvisionOperation provisionOperation(const String &value) {
            if (value == "info") {
                return ProvisionOperation::PROVISION_INFO;
            }
            if (value == "scan") {
                return ProvisionOperation::PROVISION_SCAN;
            }
            if (value == "list") {
                return ProvisionOperation::PROVISION_LIST;
            }
            if (value == "configure") {
                return ProvisionOperation::PROVISION_CONFIGURE;
            }
            if (value == "forget") {
                return ProvisionOperation::PROVISION_FORGET;
            }
            return ProvisionOperation::PROVISION_UNKNOWN;
        }
        const Route &route(Resource resource) {
            static const Route routes[] = {
                {"", "", 0, 0, ""},
                {"/info", "", 0, 0, ""},
                {"/claim", "", 0, 0, ""},
                {"/command", "command", 10, 10, ""},
                {"/device-info", "device-info", 0, 20, ""},
                {"/bytecode", "bytecode", 10, 1024, ""},
                {"/bytecode-result", "", 20, 20, ""},
                {"/debug-control", "debug-control", 8, 20, ""},
                {"/debug-result", "", 20, 20, ""},
                {"/debug-event", "", 20, 20, ""},
                {"/debug-data", "", 1, 20, ""},
                {"/events", "", 0, 0, ""},
                {"/release", "", 0, 0, ""},
                {"/capture", "", 0, 1048576, "image/jpeg"},
            };
            return routes[static_cast<size_t>(resource)];
        }
        Resource resource(const String &path) {
            if (path == "/info") {
                return Resource::Info;
            }
            if (path == "/claim") {
                return Resource::Claim;
            }
            if (path == "/command") {
                return Resource::Command;
            }
            if (path == "/device-info") {
                return Resource::DeviceInfo;
            }
            if (path == "/bytecode") {
                return Resource::Bytecode;
            }
            if (path == "/bytecode-result") {
                return Resource::BytecodeResult;
            }
            if (path == "/debug-control") {
                return Resource::DebugControl;
            }
            if (path == "/debug-result") {
                return Resource::DebugResult;
            }
            if (path == "/debug-event") {
                return Resource::DebugEvent;
            }
            if (path == "/debug-data") {
                return Resource::DebugData;
            }
            if (path == "/events") {
                return Resource::Events;
            }
            if (path == "/release") {
                return Resource::Release;
            }
            if (path == "/capture") {
                return Resource::Capture;
            }
            return Resource::Unknown;
        }
    }
    /**
     * @brief 定義済みの認証後HTTP/WS書込みを既存コールバックへ配送する。
     * @param path 署名検証に使ったパス。
     * @param payload 既存の16進本文。
     * @param sessionGeneration 検証済み接続世代。
     * @return 内容と現在世代が一致して配送できたらtrue。
     */
    bool WifiNetwork::dispatchControl(const String &path, const String &payload, uint32_t sessionGeneration) {
        if (!connected.load() || sessionGeneration != generation.load()) {
            return false;
        }
        const auto resource = jsonwire::resource(path);
        const size_t length = payload.length() / 2;
        switch (resource) {
            case jsonwire::Resource::Command: {
                if (payload.length() % 2 || length < 10 || length > 10) {
                    return false;
                }
                uint8_t buffer[10];
                uint8_t *data = buffer;
                if (!unhex(payload, data, length)) {
                    return false;
                }
                callbacks_.command(data, length, sessionGeneration);
                return true;
            }
            case jsonwire::Resource::DeviceInfo: {
                if (payload.length() % 2 || length < 0 || length > 20 || !callbacks_.deviceInfo) {
                    return false;
                }
                std::unique_ptr<uint8_t[]> storage(new (std::nothrow) uint8_t[20]{});
                if (!storage) {
                    return false;
                }
                uint8_t *data = storage.get();
                if (!unhex(payload, data, length)) {
                    return false;
                }
                callbacks_.deviceInfo(data, length, sessionGeneration);
                return true;
            }
            case jsonwire::Resource::Bytecode: {
                if (payload.length() % 2 || length < 10 || length > 1024 || !callbacks_.bytecode) {
                    return false;
                }
                std::unique_ptr<uint8_t[]> storage(new (std::nothrow) uint8_t[1024]{});
                if (!storage) {
                    return false;
                }
                uint8_t *data = storage.get();
                if (!unhex(payload, data, length)) {
                    return false;
                }
                callbacks_.bytecode(data, length, sessionGeneration);
                return true;
            }
            case jsonwire::Resource::DebugControl: {
                if (payload.length() % 2 || length < 8 || length > 20 || !callbacks_.debug) {
                    return false;
                }
                std::unique_ptr<uint8_t[]> storage(new (std::nothrow) uint8_t[20]{});
                if (!storage) {
                    return false;
                }
                uint8_t *data = storage.get();
                if (!unhex(payload, data, length)) {
                    return false;
                }
                callbacks_.debug(data, length, sessionGeneration);
                return true;
            }
            default:
                return false;
        }
    }
    /**
     * @brief 定義済みの独立Read資源を選ぶ。
     * @param path 署名検証に使ったパス。
     * @return 資源がなければnullptr。内容・世代は呼出元がロック下で検査する。
     */
    const WifiNetwork::ControlValue *WifiNetwork::generatedControlValue(const String &path) const {
        switch (jsonwire::resource(path)) {
            case jsonwire::Resource::BytecodeResult:
                return &bytecodeResult_;
            case jsonwire::Resource::DebugResult:
                return &debugResult_;
            case jsonwire::Resource::DebugEvent:
                return &debugEvent_;
            case jsonwire::Resource::DebugData:
                return &debugData_;
            default:
                return nullptr;
        }
    }
    /**
     * @brief 定義から既存のHTTP窓口とOPTIONSを登録する。
     */
    void WifiNetwork::registerGeneratedControlRoutes() {
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Info).path, HTTP_GET, &WifiNetwork::info);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Info).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Claim).path, HTTP_POST, &WifiNetwork::claimHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Claim).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Command).path, HTTP_POST, &WifiNetwork::controlHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Command).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DeviceInfo).path, HTTP_POST, &WifiNetwork::controlHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DeviceInfo).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Bytecode).path, HTTP_POST, &WifiNetwork::controlHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Bytecode).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::BytecodeResult).path, HTTP_GET, &WifiNetwork::controlValueHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::BytecodeResult).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugControl).path, HTTP_POST, &WifiNetwork::controlHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugControl).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugResult).path, HTTP_GET, &WifiNetwork::controlValueHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugResult).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugEvent).path, HTTP_GET, &WifiNetwork::controlValueHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugEvent).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugData).path, HTTP_GET, &WifiNetwork::controlValueHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::DebugData).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Events).path, HTTP_GET, &WifiNetwork::eventsHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Events).path, HTTP_OPTIONS, &WifiNetwork::options);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Release).path, HTTP_POST, &WifiNetwork::releaseHTTP);
        registerRoute(httpServer, jsonwire::route(jsonwire::Resource::Release).path, HTTP_OPTIONS, &WifiNetwork::options);
    }
    /**
     * @brief 定義から既存のHTTP窓口とOPTIONSを登録する。
     */
    void WifiNetwork::registerGeneratedCameraRoutes() {
        registerRoute(cameraServer, jsonwire::route(jsonwire::Resource::Capture).path, HTTP_GET, &WifiNetwork::imageHTTP);
        registerRoute(cameraServer, jsonwire::route(jsonwire::Resource::Capture).path, HTTP_OPTIONS, &WifiNetwork::options);
    }
}
