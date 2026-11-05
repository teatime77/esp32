/**
 * @file control_transport.h
 * @brief 通常指令の接続照合とStatus・Telemetryの共通配送窓口。
 * @details main.cppは指令キュー・機器操作・応答生成を担当し、本クラスはBLE/Wi-Fiを選別する。
 * lib/bleはGATT、lib/wifiは認証済みHTTP/WebSocketと送信バッファーを担当する。
 * Web側の共通窓口はuroa/diagram/ts/ble-wifi/control-connection.ts。
 * Command/Status v1とTelemetry v3の形式は維持し、カメラ・機器登録・Python転送・デバッグは扱わない。
 */
#pragma once
#include <uroa_ble.h>
#include <uroa_wifi.h>

/**
 * @brief 通常通信の接続照合・通知配送を通信経路から切り離す。
 * @details インスタンスと参照先は稼働中保持する。接続状態は各ライブラリーのatomicを参照し、
 * 本クラスでコピーした状態を正本にしない。publishはloop()だけから呼ぶ。
 */
class ControlTransport {
    public:
    /** @brief 同じ世代番号のBLEとWi-Fiを区別する受信元。 */
    enum class Source : uint8_t { Ble, Wifi };

    /** @brief 通信タスクでコピーし、loop()の実行直前にも照合する受信元と接続世代。 */
    struct Session {
        Source source;
        uint32_t generation;
    };

    /** @brief 稼働中保持する通信先を参照する。@param ble BLE。@param wifi Wi-Fi。 */
    ControlTransport(uroa::BleTransport &ble, uroa::WifiNetwork &wifi);

    /**
     * @brief 制御権・接続・受信元の世代を現在値と照合する。
     * @param session 受信時にコピーした通信元と世代。
     * @return Wi-Fi制御中のBLE・旧接続・切断済みを除外できたらtrue。
     */
    bool isCurrent(const Session &session) const;

    /**
     * @brief 通知長が現在の通信経路で送信可能か調べる。
     * @param length Telemetry全体の長さ。
     * @return 容量内で、BLE利用時は合意MTUからATTヘッダーを除いた範囲ならtrue。
     */
    bool canPublishTelemetry(size_t length) const;

    /**
     * @brief StatusをBLEのRead/NotifyとWi-FiのACKキューへ渡す。
     * @param data 組立て済みのStatus v1。
     * @param length 12バイト。
     * @details BLE停止予定中はGATTに触れない。呼出し完了は相手の受信を保証しない。
     */
    void publishStatus(const uint8_t *data, size_t length);

    /**
     * @brief 同じTelemetryをBLE通知とWi-Fiのセンサー通知保持へ渡す。
     * @param data 組立て済みのTelemetry v3。
     * @param length 可変長の通知全体。
     * @details Statusと混在させず、HTTPでは最新1件、WebSocketでは通知キューを利用する。
     */
    void publishTelemetry(const uint8_t *data, size_t length);

    private:
    uroa::BleTransport &ble_;
    uroa::WifiNetwork &wifi_;
};
