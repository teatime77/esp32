/**
 * @file network.cpp
 * @brief 共通Wi-Fiライブラリーへ教材の設定とカメラ処理を渡す。
 */
#include "network.h"
#include "camera.h"
#include "config.h"

uroa::WifiNetwork network;

/**
 * @brief 教材の設定とカメラ処理を渡し、Wi-Fiタスクを開始する。
 * @param callbacks 指令・接続変更・BLE開始停止の通知先。
 */
void beginNetwork(const uroa::WifiNetwork::Callbacks &callbacks) {
    uroa::WifiNetwork::Config config;
    config.name = DEVICE_NAME;
    config.connectionKey = TEACHING_CONNECTION_KEY;
    config.nvsNamespace = "uroa-net";
    auto hooks = callbacks;
    hooks.cameraBegin = [] {
        cameraBegin();
    };
    // ライブラリーのコールバックへ公開atomicの参照を接続する。
    hooks.cameraAvailable = [] {
        return cameraReady.load();
    };
    hooks.cameraServe = [](httpd_req_t *request) {
        cameraServe(request);
    };
    network.begin(config, hooks);
}
