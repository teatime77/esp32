/**
 * @file control_transport.cpp
 * @brief BLE/Wi-Fiの接続世代の照合と通常通知の配送を実装する。
 */
#include "control_transport.h"

static_assert(uroa::BleTransport::DeviceInfoMaxLength == uroa::WifiNetwork::DeviceInfoMaxLength,
    "DeviceInfo capacity must match between BLE and Wi-Fi");

/** @brief 稼働中保持する通信先を接続する。@param ble BLE。@param wifi Wi-Fi。 */
ControlTransport::ControlTransport(uroa::BleTransport &ble, uroa::WifiNetwork &wifi) : ble_(ble), wifi_(wifi) {
}

/** @brief 受信元と世代を現在の制御権へ照合する。@param session 受信時の情報。@return 現接続ならtrue。 */
bool ControlTransport::isCurrent(const Session &session) const {
    if (session.source == Source::Wifi) {
        return wifi_.connected.load() && session.generation == wifi_.generation.load();
    }
    return !wifi_.connected.load() && !wifi_.bleUnavailable() && ble_.connected.load() &&
        session.generation == ble_.generation.load();
}

/** @brief 通知容量とBLEの合意MTUを確認する。@param length 通知長。@return 送信可能ならtrue。 */
bool ControlTransport::canPublishTelemetry(size_t length) const {
    return length > 0 && length <= uroa::WifiNetwork::MaxPacketLength &&
        (wifi_.connected.load() || length + uroa::BleTransport::NotificationHeaderLength <= ble_.mtu.load());
}

/** @brief 通常指令の受付結果を配送する。@param data Status。@param length 12。 */
void ControlTransport::publishStatus(const uint8_t *data, size_t length) {
    if (!wifi_.bleUnavailable()) {
        ble_.publishStatus(data, length);
    }
    wifi_.publishStatus(data, length);
}

/** @brief センサー通知を配送する。@param data Telemetry。@param length 通知全体の長さ。 */
void ControlTransport::publishTelemetry(const uint8_t *data, size_t length) {
    if (!wifi_.bleUnavailable()) {
        ble_.publishTelemetry(data, length);
    }
    wifi_.publishTelemetry(data, length);
}
