/**
 * @file uroa_protocol.cpp
 * @brief diagramと共有するprotocol v1と機器種別付きTelemetryのパケットを読み書きする。
 * @details Command復号・通常形式検査とStatus符号化は生成codecを呼ぶ。
 * 優先STOPは従来の生バイト条件で判定する。C++構造体のパディングを通信へ含めない。
 */
#include "uroa_protocol.h"

namespace uroa {
    /** @brief 符号付き16bit値へ変換する。@param bytes 先頭バイト。@return 符号付き値。 */
    int16_t Protocol::readSigned16(const uint8_t *bytes) {
        const uint16_t value = read16(bytes);
        return value >= 0x8000 ? int32_t(value) - 0x10000 : value;
    }

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
     * @brief 生成codecで全長一致時だけ指令を復号する。不正形式の値も保持する。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さが不正な場合はversion=0の指令。
     */
    Protocol::Command Protocol::command(const uint8_t *data, size_t length) {
        return rpc::decodeCommandPacket(data, length);
    }

    /**
     * @brief 指令のversion、予約バイトと操作番号を検査する。
     * @param command 処理対象の指令。
     * @return 指令形式が正しければtrue。値の範囲は教材側で検査する。
     */
    bool Protocol::valid(const Command &command) {
        return rpc::validateCommandPacket(command);
    }

    /**
     * @brief 通常キューを迂回するSTOP指令か検査する。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さ・version・操作番号がSTOPに一致すればtrue。
     */
    bool Protocol::urgentStop(const uint8_t *data, size_t length) {
        using Layout = rpc::CommandPacketLayout;
        return length == Layout::ByteLength && data[Layout::version] == Layout::versionValue &&
            data[Layout::opcode] == STOP;
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
        const rpc::StatusPacket packet =
            rpc::makeStatusPacket(code, sequence, flags, channel, current, target, remaining);
        rpc::encodeStatusPacket(out, packet);
    }

    /**
     * @brief DeviceInfo順のセンサー種別から通知長を計算する。
     * @param sensorTypes サーボを除いたセンサー種別の配列。
     * @param sensorCount センサーの件数。
     * @return 必要なバイト数。不正な配列・未対応種別・サイズのオーバーフローでは0。
     */
    size_t Protocol::deviceTelemetryLength(const uint8_t *sensorTypes, size_t sensorCount) {
        if (sensorCount > 0 && sensorTypes == nullptr) {
            return 0;
        }
        size_t length = DeviceTelemetryHeaderLength;
        for (size_t index = 0; index < sensorCount; ++index) {
            size_t recordLength = 0;
            switch (sensorTypes[index]) {
                case IMU6AxisDeviceType:
                    recordLength = IMU6AxisRecordLength;
                    break;
                case UltrasonicDeviceType:
                    recordLength = UltrasonicRecordLength;
                    break;
                default:
                    return 0;
            }
            if (length > SIZE_MAX - recordLength) {
                return 0;
            }
            length += recordLength;
        }
        return length;
    }

    /**
     * @brief version 3のヘッダーと、DeviceInfo順のIMU・距離レコードを作る。
     * @param out 出力バッファー。
     * @param capacity 出力領域のバイト数。
     * @param flags 機器・制御状態。
     * @param sequence 通知のsequence。
     * @param now 通知を組み立てるサンプル時刻millis。
     * @param sensorTypes サーボを除いたセンサー種別の配列。
     * @param sensorCount センサーの件数。
     * @param imuRaw MPU-6050の14バイト。IMUを含まなければnullptrも可。
     * @param distanceMm 20〜4000mm、または未測定・失敗を示すInvalidDistanceMm。
     * @return 出力バイト数。引数不正・未対応種別・容量不足なら0で、出力は変更しない。
     */
    size_t Protocol::deviceTelemetry(uint8_t *out, size_t capacity, uint8_t flags, uint16_t sequence, uint32_t now,
        const uint8_t *sensorTypes, size_t sensorCount, const uint8_t *imuRaw, uint16_t distanceMm) {
        const size_t length = deviceTelemetryLength(sensorTypes, sensorCount);
        if (out == nullptr || length == 0 || capacity < length) {
            return 0;
        }
        // 全レコードを検査してから書く。後半の引数が不正でも部分的な通知を残さない。
        for (size_t index = 0; index < sensorCount; ++index) {
            if (sensorTypes[index] == IMU6AxisDeviceType && imuRaw == nullptr) {
                return 0;
            }
            if (sensorTypes[index] == UltrasonicDeviceType && distanceMm != InvalidDistanceMm &&
                (distanceMm < MinDistanceMm || distanceMm > MaxDistanceMm)) {
                return 0;
            }
        }
        const rpc::TelemetryHeader header{DeviceTelemetryVersion, flags, sequence, now};
        rpc::encodeTelemetryHeader(out, header);
        size_t offset = DeviceTelemetryHeaderLength;
        for (size_t index = 0; index < sensorCount; ++index) {
            out[offset] = sensorTypes[index];
            if (sensorTypes[index] == IMU6AxisDeviceType) {
                using Layout = rpc::ImuRecordLayout;
                for (size_t axis = 0; axis < 3; ++axis) {
                    out[offset + Layout::accX + 2 * axis] = imuRaw[2 * axis + 1];
                    out[offset + Layout::accX + 1 + 2 * axis] = imuRaw[2 * axis];
                    out[offset + Layout::gyroX + 2 * axis] = imuRaw[9 + 2 * axis];
                    out[offset + Layout::gyroX + 1 + 2 * axis] = imuRaw[8 + 2 * axis];
                }
                offset += IMU6AxisRecordLength;
            }
            else {
                write16(out + offset + rpc::DistanceRecordLayout::distanceMm, distanceMm);
                offset += UltrasonicRecordLength;
            }
        }
        return offset;
    }
}
