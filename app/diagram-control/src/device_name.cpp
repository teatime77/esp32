/**
 * @file device_name.cpp
 * @brief 機種名と工場出荷時のMACアドレスから基板固有の表示名を生成する。
 * @details config.hのDEVICE_NAME_PREFIXに「-」とeFuseの既定ベースMAC全6バイトを
 * 大文字16進12桁で付加する。再起動・再書込み・NVS消去でも同じチップの番号は変わらない。
 * main.cppのsetup()で一度生成し、BleTransportとnetwork.cppのWifiNetworkへ同じ名前を渡す。
 * BLEのScan Responseへ名前全体を格納できるよう、名前はASCIIの最大26バイトとする。
 * MAC取得失敗時は空文字列を返し、setup()が通信開始前に停止する。
 * この番号は表示用で認証情報ではない。Wi-Fiの保存済みdeviceId・hostnameとUUIDは変更しない。
 */
#include "device_name.h"
#include "config.h"
#include <esp_mac.h>
#include <cstdio>

static_assert(sizeof(DEVICE_NAME_PREFIX) - 1 + 1 + 12 <= 26, "Device name exceeds scan response budget");

/**
 * @brief 機種名へ工場出荷時のMAC全体を付加し、表示名を生成する。
 * @return 基板固有の表示名。MAC取得に失敗した場合は空文字列。
 */
String makeDeviceName() {
    uint8_t mac[6]{};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        return String();
    }
    char suffix[14]{};
    std::snprintf(suffix, sizeof(suffix), "-%02X%02X%02X%02X%02X%02X",
        static_cast<unsigned int>(mac[0]), static_cast<unsigned int>(mac[1]),
        static_cast<unsigned int>(mac[2]), static_cast<unsigned int>(mac[3]),
        static_cast<unsigned int>(mac[4]), static_cast<unsigned int>(mac[5]));
    return String(DEVICE_NAME_PREFIX) + suffix;
}
