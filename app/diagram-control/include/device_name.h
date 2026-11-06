/**
 * @file device_name.h
 * @brief 基板固有番号を含むBLE／Wi-Fi表示名の生成窓口。
 */
#pragma once
#include <Arduino.h>

/**
 * @brief 工場出荷時のMACアドレスを使い、基板ごとの表示名を生成する。
 * @return 機種名と固有番号を含む文字列。MACを取得できない場合は空文字列。
 * @details setup()から呼ぶ。表示名の形式と通信への反映先はdevice_name.cppを参照する。
 */
String makeDeviceName();
