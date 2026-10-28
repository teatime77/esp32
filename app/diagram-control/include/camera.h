/**
 * @file camera.h
 * @brief XIAO ESP32-S3 SenseのJPEGカメラと公開状態。
 */
#pragma once
#include <esp_http_server.h>
#include <atomic>

extern std::atomic<bool> cameraReady; ///< カメラ初期化の結果。HTTP側はload()で参照し、更新はcameraBegin()に任せる。

/**
 * @brief XIAO Senseのカメラを初期化する。開始済みなら再初期化しない。
 * @return 利用可能ならtrue。非対応基板、PSRAMなし、初期化失敗ではfalse。
 */
bool cameraBegin();

/**
 * @brief 1枚のJPEGを返し、利用できない場合はHTTP 503を返す。
 * @param request HTTPサーバーから渡された有効な要求。
 * @details 認証とCORSはWifiNetworkが処理し、画像バッファーは応答後に返却する。
 */
void cameraServe(httpd_req_t *request);
