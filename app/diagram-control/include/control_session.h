/**
 * @file control_session.h
 * @brief 通信実装へ依存しない制御セッションとloop()の指令コンテキスト。
 */
#pragma once
#include <stdint.h>

/**
 * @brief 同じ世代番号のBLEとWi-Fiを区別する受信元。
 */
enum class ControlSource : uint8_t {
    Ble,
    Wifi
};

/**
 * @brief 通信タスクでコピーし、loop()で現在の接続へ再照合する受信元と世代。
 * @details 接続状態の正本は各通信ライブラリーのatomic。制御権の検査はControlTransportが行う。
 */
struct ControlSession {
    ControlSource source; ///< 受信元の通信経路。世代番号が同じBLEとWi-Fiも区別する。
    uint32_t generation; ///< 受信時の接続世代。loop()が現在の通信状態と再照合する。
};

/**
 * @brief 生成した振分けから機能本体へ同期的に渡すloop()の実行状態。
 * @details sessionは呼出し中だけ借用する。保存するVM開始元は機能側で値としてコピーする。
 * 受信時とloop()での接続照合に加え、VM操作の直前にも機能側で現在の制御権を照合する。
 */
struct ControlRpcContext {
    const ControlSession &session; ///< 指令の受信元と世代。
    bool allowVmStart; ///< 停止要求を処理した周回ではfalse。
};
