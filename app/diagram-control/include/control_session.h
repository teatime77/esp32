/**
 * @file control_session.h
 * @brief 通信実装へ依存しない制御セッションの識別子。
 */
#pragma once
#include <stdint.h>

/** @brief 同じ世代番号のBLEとWi-Fiを区別する受信元。 */
enum class ControlSource : uint8_t { Ble, Wifi };

/**
 * @brief 通信タスクでコピーし、loop()で現在の接続へ再照合する受信元と世代。
 * @details 接続状態の正本は各通信ライブラリーのatomic。制御権の検査はControlTransportが行う。
 */
struct ControlSession {
    ControlSource source;
    uint32_t generation;
};
