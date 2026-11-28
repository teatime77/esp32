/**
 * @file uroa_protocol.cpp
 * @brief diagramと共有するprotocol v1と機器種別付きTelemetryのパケットを読み書きする。
 * @details 指令とStatusのcodec・番号の互換窓口は持たず、利用側が生成APIを直接使う。
 * 優先STOPは従来の生バイト条件で判定する。C++構造体のパディングを通信へ含めない。
 * Telemetryの種別・全長・値の検査と書込みはwire.protoの生成レコード列へ委譲する。
 * MPUの測定値の軸順変換は本ファイル、計測・登録順・送信先は教材側が担当する。
 */
#include "uroa_protocol.h"
#include "uroa_wire.generated.h"

namespace uroa {
    /**
     * @brief 通常キューを迂回するSTOP指令か検査する。
     * @param data 入力または出力のバイト配列。
     * @param length データ長（バイト）。
     * @return 長さ・version・操作番号がSTOPに一致すればtrue。
     */
    bool Protocol::urgentStop(const uint8_t *data, size_t length) {
        using Layout = rpc::CommandPacketLayout;
        return length == Layout::ByteLength && data[Layout::version] == Layout::versionValue &&
            data[Layout::opcode] == static_cast<uint8_t>(rpc::CommandOpcode::STOP);
    }

    /**
     * @brief DeviceInfo順のセンサー種別から通知長を計算する。
     * @param sensorTypes サーボを除いたセンサー種別の配列。
     * @param sensorCount センサーの件数。
     * @return 必要なバイト数。不正な配列・未対応種別・サイズのオーバーフローでは0。
     */
    size_t Protocol::deviceTelemetryLength(const uint8_t *sensorTypes, size_t sensorCount) {
        return wire::TelemetryRecordsLength(sensorTypes, sensorCount);
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
        if (out == nullptr || (sensorCount && sensorTypes == nullptr)) {
            return 0;
        }
        wire::ImuRecord imu{};
        imu.deviceType = IMU6AxisDeviceType;
        bool usesImu = false;
        for (size_t index = 0; index < sensorCount; ++index) {
            if (sensorTypes[index] == IMU6AxisDeviceType) {
                usesImu = true;
            }
        }
        if (usesImu) {
            if (imuRaw == nullptr) {
                return 0;
            }
            // MPU-6050のレジスター順はbig endian。温度2バイトを除いて値へ変換する。
            const auto sensor16 = [](const uint8_t *raw) -> int16_t {
                const uint16_t bits = (uint16_t(raw[0]) << 8) | raw[1];
                return bits >= 0x8000 ? int32_t(bits) - 0x10000 : bits;
            };
            imu.accX = sensor16(imuRaw);
            imu.accY = sensor16(imuRaw + 2);
            imu.accZ = sensor16(imuRaw + 4);
            imu.gyroX = sensor16(imuRaw + 8);
            imu.gyroY = sensor16(imuRaw + 10);
            imu.gyroZ = sensor16(imuRaw + 12);
        }
        const wire::DistanceRecord distance{UltrasonicDeviceType, distanceMm};
        const wire::TelemetryHeader header{DeviceTelemetryVersion, flags, sequence, now};
        const wire::TelemetryRecordsValues values{imu, distance};
        return wire::writeTelemetryRecords(out, capacity, header, sensorTypes, sensorCount, values);
    }
}
