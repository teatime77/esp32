/**
 * @file camera.cpp
 * @brief XIAO ESP32-S3 Senseの320×240 JPEG画像を取得する。
 */
#include "camera.h"
#include <Arduino.h>
#if defined(ARDUINO_XIAO_ESP32S3)
#include <esp_camera.h>
#endif

std::atomic<bool> cameraReady{false};

/**
 * @brief XIAO Senseのカメラを初期化する。開始済みなら再初期化しない。
 * @return 利用可能ならtrue。非対応基板、PSRAMなし、初期化失敗ではfalse。
 */
bool cameraBegin() {
    if (cameraReady.load()) {
        return true;
    }
#if defined(ARDUINO_XIAO_ESP32S3)
    if (!psramFound()) {
        return false;
    }
    camera_config_t config{};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 15;
    config.pin_d1 = 17;
    config.pin_d2 = 18;
    config.pin_d3 = 16;
    config.pin_d4 = 14;
    config.pin_d5 = 12;
    config.pin_d6 = 11;
    config.pin_d7 = 48;
    config.pin_xclk = 10;
    config.pin_pclk = 13;
    config.pin_vsync = 38;
    config.pin_href = 47;
    config.pin_sccb_sda = 40;
    config.pin_sccb_scl = 39;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 16;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    cameraReady.store(esp_camera_init(&config) == ESP_OK);
#endif
    return cameraReady.load();
}

/**
 * @brief 1枚のJPEGを返し、利用できない場合はHTTP 503を返す。
 * @param request HTTPサーバーから渡された有効な要求。
 */
void cameraServe(httpd_req_t *request) {
#if defined(ARDUINO_XIAO_ESP32S3)
    if (cameraReady.load()) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame) {
            httpd_resp_set_type(request, "image/jpeg");
            httpd_resp_send(request, reinterpret_cast<const char *>(frame->buf), frame->len);
            esp_camera_fb_return(frame);
            return;
        }
    }
#endif
    httpd_resp_set_status(request, "503 Service Unavailable");
    httpd_resp_send(request, "Camera unavailable", HTTPD_RESP_USE_STRLEN);
}
