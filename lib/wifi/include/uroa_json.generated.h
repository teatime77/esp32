/**
 * @file uroa_json.generated.h
 * @brief transport.protoのJSONフィールドと窓口。手動編集しない。
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
namespace uroa {
    namespace jsonwire {
        /**
         * @brief EndpointFieldsのJSON値。文字列と配列はDocumentを借用する。
         */
        struct EndpointFields {
            const char *id; ///< JSONの値。
            const char *name; ///< JSONの値。
            const char *host; ///< JSONの値。
            const char *ip; ///< JSONの値。
            uint32_t port; ///< JSONの値。
            uint32_t wsPort; ///< JSONの値。
            uint32_t cameraPort; ///< JSONの値。
            bool camera; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        EndpointFields readEndpointFields(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param id idの値。
         * @param name nameの値。
         * @param host hostの値。
         * @param ip ipの値。
         * @param port portの値。
         * @param wsPort wsPortの値。
         * @param cameraPort cameraPortの値。
         * @param camera cameraの値。
         */
        void writeEndpointFields(JsonObject out, const String &id, const String &name, const String &host, const String &ip, uint32_t port, uint32_t wsPort, uint32_t cameraPort, bool camera);
        /**
         * @brief ProvisionStatusのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ProvisionStatus {
            const char *state; ///< JSONの値。
            const char *error; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ProvisionStatus readProvisionStatus(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param state stateの値。
         * @param error errorの値。
         */
        void writeProvisionStatus(JsonObject out, const String &state, const String &error);
        /**
         * @brief SecretFieldのJSON値。文字列と配列はDocumentを借用する。
         */
        struct SecretField {
            const char *key; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        SecretField readSecretField(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param key keyの値。
         */
        void writeSecretField(JsonObject out, const String &key);
        /**
         * @brief ProvisionQueryのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ProvisionQuery {
            const char *op; ///< JSONの値。
            int32_t page; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ProvisionQuery readProvisionQuery(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param op opの値。
         * @param page pageの値。
         */
        void writeProvisionQuery(JsonObject out, const String &op, int32_t page);
        /**
         * @brief ProvisionListのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ProvisionList {
            const char *op; ///< JSONの値。
            uint32_t page; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ProvisionList readProvisionList(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param page pageの値。
         */
        void writeProvisionList(JsonObject out, uint32_t page);
        /**
         * @brief ProvisionConfigureのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ProvisionConfigure {
            const char *op; ///< JSONの値。
            const char *ssid; ///< JSONの値。
            const char *password; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ProvisionConfigure readProvisionConfigure(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param ssid ssidの値。
         * @param password passwordの値。
         */
        void writeProvisionConfigure(JsonObject out, const String &ssid, const String &password);
        /**
         * @brief WifiNetworkRowのJSON値。文字列と配列はDocumentを借用する。
         */
        struct WifiNetworkRow {
            const char *ssid; ///< JSONの値。
            int32_t rssi; ///< JSONの値。
            bool open; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        WifiNetworkRow readWifiNetworkRow(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param ssid ssidの値。
         * @param rssi rssiの値。
         * @param open openの値。
         */
        void writeWifiNetworkRow(JsonObject out, const String &ssid, int32_t rssi, bool open);
        /**
         * @brief HttpClaimのJSON値。文字列と配列はDocumentを借用する。
         */
        struct HttpClaim {
            const char *challenge; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        HttpClaim readHttpClaim(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param challenge challengeの値。
         * @param proof proofの値。
         */
        void writeHttpClaim(JsonObject out, const String &challenge, const String &proof);
        /**
         * @brief SocketClaimのJSON値。文字列と配列はDocumentを借用する。
         */
        struct SocketClaim {
            const char *op; ///< JSONの値。
            const char *challenge; ///< JSONの値。
            const char *proof; ///< JSONの値。
            const char *origin; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        SocketClaim readSocketClaim(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param challenge challengeの値。
         * @param proof proofの値。
         * @param origin originの値。
         */
        void writeSocketClaim(JsonObject out, const String &challenge, const String &proof, const String &origin);
        /**
         * @brief ReadyFieldsのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ReadyFields {
            const char *session; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ReadyFields readReadyFields(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param session sessionの値。
         * @param proof proofの値。
         */
        void writeReadyFields(JsonObject out, const String &session, const String &proof);
        /**
         * @brief SocketCommandのJSON値。文字列と配列はDocumentを借用する。
         */
        struct SocketCommand {
            const char *op; ///< JSONの値。
            const char *session; ///< JSONの値。
            uint32_t counter; ///< JSONの値。
            const char *data; ///< JSONの値。
            const char *mac; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        SocketCommand readSocketCommand(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param op opの値。
         * @param session sessionの値。
         * @param counter counterの値。
         * @param data dataの値。
         * @param mac macの値。
         */
        void writeSocketCommand(JsonObject out, const String &op, const String &session, uint32_t counter, const String &data, const String &mac);
        /**
         * @brief ReadReplyのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ReadReply {
            const char *session; ///< JSONの値。
            const char *counter; ///< JSONの値。
            const char *data; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ReadReply readReadReply(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param session sessionの値。
         * @param counter counterの値。
         * @param data dataの値。
         * @param proof proofの値。
         */
        void writeReadReply(JsonObject out, const String &session, const String &counter, const String &data, const String &proof);
        /**
         * @brief SignedFieldsのJSON値。文字列と配列はDocumentを借用する。
         */
        struct SignedFields {
            const char *session; ///< JSONの値。
            const char *counter; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        SignedFields readSignedFields(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param session sessionの値。
         * @param counter counterの値。
         * @param proof proofの値。
         */
        void writeSignedFields(JsonObject out, const String &session, const String &counter, const String &proof);
        /**
         * @brief SocketPacketのJSON値。文字列と配列はDocumentを借用する。
         */
        struct SocketPacket {
            const char *data; ///< JSONの値。
            uint32_t counter; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        SocketPacket readSocketPacket(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param data dataの値。
         * @param counter counterの値。
         * @param proof proofの値。
         */
        void writeSocketPacket(JsonObject out, const String &data, uint32_t counter, const String &proof);
        /**
         * @brief EventsReplyのJSON値。文字列と配列はDocumentを借用する。
         */
        struct EventsReply {
            JsonArrayConst packets; ///< JSONの値。
            const char *session; ///< JSONの値。
            const char *counter; ///< JSONの値。
            const char *proof; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        EventsReply readEventsReply(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param packets packetsの値。
         * @param session sessionの値。
         * @param counter counterの値。
         * @param proof proofの値。
         */
        void writeEventsReply(JsonObject out, const String &packets, const String &session, const String &counter, const String &proof);
        extern const char AcceptedJson[]; ///< 宣言した固定JSON。
        /**
         * @brief AcceptedのJSON値。文字列と配列はDocumentを借用する。
         */
        struct Accepted {
            bool accepted; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        Accepted readAccepted(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         */
        void writeAccepted(JsonObject out);
        /**
         * @brief RawBytesのJSON値。文字列と配列はDocumentを借用する。
         */
        struct RawBytes {
            const char *data; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        RawBytes readRawBytes(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param data dataの値。
         */
        void writeRawBytes(JsonObject out, const String &data);
        /**
         * @brief ChallengeFieldsのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ChallengeFields {
            const char *challenge; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ChallengeFields readChallengeFields(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param challenge challengeの値。
         */
        void writeChallengeFields(JsonObject out, const String &challenge);
        /**
         * @brief ListPagingのJSON値。文字列と配列はDocumentを借用する。
         */
        struct ListPaging {
            uint32_t pages; ///< JSONの値。
        };
        /**
         * @brief Document内の値を借用して読む。
         * @param input 解析済みJSON。
         * @return 借用した値。寿命を越えて保存しない。
         */
        ListPaging readListPaging(JsonVariantConst input);
        /**
         * @brief 宣言順のJSONフィールドを書く。
         * @param out 出力オブジェクト。
         * @param pages pagesの値。
         */
        void writeListPaging(JsonObject out, uint32_t pages);
        /**
         * @brief BLE設定JSONの操作。
         */
        enum class ProvisionOperation {
            PROVISION_UNKNOWN = 0,
            PROVISION_INFO = 1,
            PROVISION_SCAN = 2,
            PROVISION_LIST = 3,
            PROVISION_CONFIGURE = 4,
            PROVISION_FORGET = 5,
        };
        /**
         * @brief 設定の操作名を読む。
         * @param value 操作名。
         * @return 不明ならPROVISION_UNKNOWN。
         */
        ProvisionOperation provisionOperation(const String &value);
        /**
         * @brief 既存HTTP/WSの資源。番号は通信へ送らない。
         */
        enum class Resource {
            Unknown,
            Info,
            Claim,
            Command,
            DeviceInfo,
            Bytecode,
            BytecodeResult,
            DebugControl,
            DebugResult,
            DebugEvent,
            DebugData,
            Events,
            Release,
            Capture,
        };
        /**
         * @brief 既存窓口のパスとサイズ。
         */
        struct Route {
            const char *path; ///< HMACにも使う既存パス。
            const char *op; ///< WS操作名。
            size_t minBytes; ///< 読み書きの下限。
            size_t maxBytes; ///< 読み書きの上限。
            const char *mime; ///< 生データのMIME。
        };
        /**
         * @brief 資源の定義を返す。
         * @param resource 資源。
         * @return 稼働中有効な定義。
         */
        const Route &route(Resource resource);
        /**
         * @brief パスを資源へ対応付ける。
         * @param path 既存パス。
         * @return 不明ならUnknown。
         */
        Resource resource(const String &path);
    }
}
