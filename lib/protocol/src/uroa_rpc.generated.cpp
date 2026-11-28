/**
 * @file uroa_rpc.generated.cpp
 * @brief device.protoから生成した固定長パケットの書込み。手動編集しない。
 */
#include "uroa_rpc.generated.h"

namespace uroa {
    namespace rpc {
        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 幅を確定した値。
         */
        void encodeCommandPacket(uint8_t *out, const CommandPacket &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[CommandPacketLayout::version] = static_cast<uint8_t>(versionValue);
            const uint32_t opcodeValue = static_cast<uint32_t>(packet.opcode);
            out[CommandPacketLayout::opcode] = static_cast<uint8_t>(opcodeValue);
            const uint32_t sequenceValue = static_cast<uint32_t>(packet.sequence);
            out[CommandPacketLayout::sequence] = static_cast<uint8_t>(sequenceValue);
            out[CommandPacketLayout::sequence + 1] = static_cast<uint8_t>(sequenceValue >> 8);
            const uint32_t channelValue = static_cast<uint32_t>(packet.channel);
            out[CommandPacketLayout::channel] = static_cast<uint8_t>(channelValue);
            const uint32_t reservedValue = static_cast<uint32_t>(packet.reserved);
            out[CommandPacketLayout::reserved] = static_cast<uint8_t>(reservedValue);
            const uint32_t valueValue = static_cast<uint32_t>(packet.value);
            out[CommandPacketLayout::value] = static_cast<uint8_t>(valueValue);
            out[CommandPacketLayout::value + 1] = static_cast<uint8_t>(valueValue >> 8);
            const uint32_t durationValue = static_cast<uint32_t>(packet.duration);
            out[CommandPacketLayout::duration] = static_cast<uint8_t>(durationValue);
            out[CommandPacketLayout::duration + 1] = static_cast<uint8_t>(durationValue >> 8);
        }

        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 幅を確定した値。
         */
        void encodeStatusPacket(uint8_t *out, const StatusPacket &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[StatusPacketLayout::version] = static_cast<uint8_t>(versionValue);
            const uint32_t codeValue = static_cast<uint32_t>(packet.code);
            out[StatusPacketLayout::code] = static_cast<uint8_t>(codeValue);
            const uint32_t sequenceValue = static_cast<uint32_t>(packet.sequence);
            out[StatusPacketLayout::sequence] = static_cast<uint8_t>(sequenceValue);
            out[StatusPacketLayout::sequence + 1] = static_cast<uint8_t>(sequenceValue >> 8);
            const uint32_t flagsValue = static_cast<uint32_t>(packet.flags);
            out[StatusPacketLayout::flags] = static_cast<uint8_t>(flagsValue);
            const uint32_t channelValue = static_cast<uint32_t>(packet.channel);
            out[StatusPacketLayout::channel] = static_cast<uint8_t>(channelValue);
            const uint32_t currentValue = static_cast<uint32_t>(packet.current);
            out[StatusPacketLayout::current] = static_cast<uint8_t>(currentValue);
            out[StatusPacketLayout::current + 1] = static_cast<uint8_t>(currentValue >> 8);
            const uint32_t targetValue = static_cast<uint32_t>(packet.target);
            out[StatusPacketLayout::target] = static_cast<uint8_t>(targetValue);
            out[StatusPacketLayout::target + 1] = static_cast<uint8_t>(targetValue >> 8);
            const uint32_t remainingValue = static_cast<uint32_t>(packet.remaining);
            out[StatusPacketLayout::remaining] = static_cast<uint8_t>(remainingValue);
            out[StatusPacketLayout::remaining + 1] = static_cast<uint8_t>(remainingValue >> 8);
        }

        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 幅を確定した値。
         */
        void encodeTelemetryHeader(uint8_t *out, const TelemetryHeader &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[TelemetryHeaderLayout::version] = static_cast<uint8_t>(versionValue);
            const uint32_t flagsValue = static_cast<uint32_t>(packet.flags);
            out[TelemetryHeaderLayout::flags] = static_cast<uint8_t>(flagsValue);
            const uint32_t sequenceValue = static_cast<uint32_t>(packet.sequence);
            out[TelemetryHeaderLayout::sequence] = static_cast<uint8_t>(sequenceValue);
            out[TelemetryHeaderLayout::sequence + 1] = static_cast<uint8_t>(sequenceValue >> 8);
            const uint32_t millisValue = static_cast<uint32_t>(packet.millis);
            out[TelemetryHeaderLayout::millis] = static_cast<uint8_t>(millisValue);
            out[TelemetryHeaderLayout::millis + 1] = static_cast<uint8_t>(millisValue >> 8);
            out[TelemetryHeaderLayout::millis + 2] = static_cast<uint8_t>(millisValue >> 16);
            out[TelemetryHeaderLayout::millis + 3] = static_cast<uint8_t>(millisValue >> 24);
        }

        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 幅を確定した値。
         */
        void encodeImuRecord(uint8_t *out, const ImuRecord &packet) {
            const uint32_t deviceTypeValue = static_cast<uint32_t>(packet.deviceType);
            out[ImuRecordLayout::deviceType] = static_cast<uint8_t>(deviceTypeValue);
            const uint32_t accXValue = static_cast<uint32_t>(packet.accX);
            out[ImuRecordLayout::accX] = static_cast<uint8_t>(accXValue);
            out[ImuRecordLayout::accX + 1] = static_cast<uint8_t>(accXValue >> 8);
            const uint32_t accYValue = static_cast<uint32_t>(packet.accY);
            out[ImuRecordLayout::accY] = static_cast<uint8_t>(accYValue);
            out[ImuRecordLayout::accY + 1] = static_cast<uint8_t>(accYValue >> 8);
            const uint32_t accZValue = static_cast<uint32_t>(packet.accZ);
            out[ImuRecordLayout::accZ] = static_cast<uint8_t>(accZValue);
            out[ImuRecordLayout::accZ + 1] = static_cast<uint8_t>(accZValue >> 8);
            const uint32_t gyroXValue = static_cast<uint32_t>(packet.gyroX);
            out[ImuRecordLayout::gyroX] = static_cast<uint8_t>(gyroXValue);
            out[ImuRecordLayout::gyroX + 1] = static_cast<uint8_t>(gyroXValue >> 8);
            const uint32_t gyroYValue = static_cast<uint32_t>(packet.gyroY);
            out[ImuRecordLayout::gyroY] = static_cast<uint8_t>(gyroYValue);
            out[ImuRecordLayout::gyroY + 1] = static_cast<uint8_t>(gyroYValue >> 8);
            const uint32_t gyroZValue = static_cast<uint32_t>(packet.gyroZ);
            out[ImuRecordLayout::gyroZ] = static_cast<uint8_t>(gyroZValue);
            out[ImuRecordLayout::gyroZ + 1] = static_cast<uint8_t>(gyroZValue >> 8);
        }

        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 幅を確定した値。
         */
        void encodeDistanceRecord(uint8_t *out, const DistanceRecord &packet) {
            const uint32_t deviceTypeValue = static_cast<uint32_t>(packet.deviceType);
            out[DistanceRecordLayout::deviceType] = static_cast<uint8_t>(deviceTypeValue);
            const uint32_t distanceMmValue = static_cast<uint32_t>(packet.distanceMm);
            out[DistanceRecordLayout::distanceMm] = static_cast<uint8_t>(distanceMmValue);
            out[DistanceRecordLayout::distanceMm + 1] = static_cast<uint8_t>(distanceMmValue >> 8);
        }

    }
}
