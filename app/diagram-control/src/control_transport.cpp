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

/** @brief 制御権と各atomicの世代を読む。@return 経路付きの識別子。状態の変化はisCurrent()で再照合する。 */
ControlTransport::Session ControlTransport::currentSession() const {
    if (wifi_.connected.load()) {
        return {Source::Wifi, wifi_.generation.load()};
    }
    return {Source::Ble, ble_.generation.load()};
}

/** @brief 専用のプログラム受付結果へ配送する。@param data 結果。@param length 20。@param session 受信元。 */
void ControlTransport::publishBytecodeResult(const uint8_t *data, size_t length, const Session &session) {
    if (!isCurrent(session)) {
        return;
    }
    if (session.source == Source::Wifi) {
        wifi_.publishBytecodeResult(data, length, session.generation);
    }
    else {
        ble_.publishBytecodeResult(data, length);
    }
}

/** @brief デバッグACKを配送する。@param data 内容。@param length 20。@param session 受信元。 */
void ControlTransport::publishDebugResult(const uint8_t *data, size_t length, const Session &session) {
    if (!isCurrent(session)) {
        return;
    }
    if (session.source == Source::Wifi) {
        wifi_.publishDebugResult(data, length, session.generation);
    }
    else {
        ble_.publishDebugResult(data, length);
    }
}

/** @brief 停止イベントを配送する。@param data 内容。@param length 20。@param session 受信元。 */
void ControlTransport::publishDebugEvent(const uint8_t *data, size_t length, const Session &session) {
    if (!isCurrent(session)) {
        return;
    }
    if (session.source == Source::Wifi) {
        wifi_.publishDebugEvent(data, length, session.generation);
    }
    else {
        ble_.publishDebugEvent(data, length);
    }
}

/** @brief 選択済み停止データを配送する。@param data 内容。@param length 1〜20。@param session 受信元。 */
void ControlTransport::publishDebugData(const uint8_t *data, size_t length, const Session &session) {
    if (!isCurrent(session)) {
        return;
    }
    if (session.source == Source::Wifi) {
        wifi_.publishDebugData(data, length, session.generation);
    }
    else {
        ble_.publishDebugData(data, length);
    }
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
