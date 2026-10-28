/**
 * @file network.h
 * @brief 教材のWi-Fi設定とカメラの接続窓口。
 */
#pragma once
#include <uroa_wifi.h>

extern uroa::WifiNetwork network; ///< 稼働中保持するWi-Fi制御サーバー。状態の更新はライブラリーに任せる。

/**
 * @brief 教材の設定とカメラ処理を渡し、Wi-Fiタスクを開始する。
 * @param callbacks 指令・接続変更・BLE開始停止の通知先。
 * @details setup()で1回呼ぶ。通信コールバックからI2Cを操作しない。
 */
void beginNetwork(const uroa::WifiNetwork::Callbacks &callbacks);
