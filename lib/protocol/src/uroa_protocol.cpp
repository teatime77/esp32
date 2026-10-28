/**
 * @file uroa_protocol.cpp
 * @brief diagramと共有するprotocol v1のパケットを読み書きする。
 */
#include "uroa_protocol.h"
#include <string.h>

namespace uroa {
    /**
     * @brief little endianの16bit値を読み取る。
     * @param b 変換する値の先頭バイト。
     * @return 符号なし16bit値。
     */
    uint16_t Protocol::read16(const uint8_t *b) {
        return uint16_t(b[0]) | uint16_t(b[1]) << 8;
    }

    /**
     * @brief 16bit値をlittle endianで書き込む。
     * @param b 変換する値の先頭バイト。
     * @param v 書き込む値。
     */
    void Protocol::write16(uint8_t *b, uint16_t v) {
        b[0] = v;
        b[1] = v >> 8;
    }

    /**
     * @brief 32bit値をlittle endianで書き込む。
     * @param b 変換する値の先頭バイト。
     * @param v 書き込む値。
     */
    void Protocol::write32(uint8_t *b, uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            b[i] = v >> (8 * i);
        }
    }

    /**
     * @brief 受信データを固定長の指令へコピーする。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さが不正な場合はversion=0の指令。
     */
    Protocol::Command Protocol::command(const uint8_t *data, size_t length) {
        Command result;
        if (length == sizeof result.bytes) {
            memcpy(result.bytes, data, length);
        }
        return result;
    }

    /**
     * @brief 指令のversion、予約バイトと操作番号を検査する。
     * @param command 処理対象の指令。
     * @return 指令形式が正しければtrue。値の範囲は教材側で検査する。
     */
    bool Protocol::valid(const Command &command) {
        const auto *b = command.bytes;
        return b[0] == 1 && b[5] == 0 && b[1] >= ARM && b[1] <= DISABLE;
    }

    /**
     * @brief 通常キューを迂回するSTOP指令か検査する。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さ・version・操作番号がSTOPに一致すればtrue。
     */
    bool Protocol::urgentStop(const uint8_t *data, size_t length) {
        return length == 10 && data[0] == 1 && data[1] == STOP;
    }

    /**
     * @brief 機器の状態と受付結果を12バイトのStatusへ詰める。
     * @param out 出力先バッファーまたはJSONオブジェクト。
     * @param code Statusの受付結果コード。
     * @param sequence 16bitで周回するsequence。
     * @param flags 機器・制御・通知の状態フラグ。
     * @param channel 出力CH番号。
     * @param current 現在の内部指令角度（0.01度）。
     * @param target 目標の内部指令角度（0.01度）。
     * @param remaining 補間の残り時間（ms）。
     */
    void Protocol::status(uint8_t *out, uint8_t code, uint16_t sequence, uint8_t flags, uint8_t channel,
        uint16_t current, uint16_t target, uint16_t remaining) {
        memset(out, 0, 12);
        out[0] = 1;
        out[1] = code;
        out[4] = flags;
        out[5] = channel;
        write16(out + 2, sequence);
        if (channel < 16) {
            write16(out + 6, current);
            write16(out + 8, target);
            write16(out + 10, remaining);
        }
    }

    /**
     * @brief IMUの14バイトから温度を除いた6軸を20バイトのTelemetryへ詰める。
     * @param out 出力先バッファーまたはJSONオブジェクト。
     * @param flags 機器・制御・通知の状態フラグ。
     * @param sequence 16bitで周回するsequence。
     * @param now 呼出元の現在時刻（ms、周回あり）。
     * @param raw 加速度・温度・角速度を含む14バイトの生データ。
     */
    void Protocol::telemetry(uint8_t *out, uint8_t flags, uint16_t sequence, uint32_t now, const uint8_t *raw) {
        out[0] = 1;
        out[1] = flags;
        write16(out + 2, sequence);
        write32(out + 4, now);
        // センサーはbig endian、通信はlittle endianなので軸ごとにバイトを交換する。
        for (int i = 0; i < 3; ++i) {
            out[8 + 2 * i] = raw[2 * i + 1];
            out[9 + 2 * i] = raw[2 * i];
            out[14 + 2 * i] = raw[9 + 2 * i];
            out[15 + 2 * i] = raw[8 + 2 * i];
        }
    }
}
