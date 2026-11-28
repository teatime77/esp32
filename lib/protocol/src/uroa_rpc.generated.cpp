/**
 * @file uroa_rpc.generated.cpp
 * @brief device.protoから生成した方向別の固定長codecと形式検査。手動編集しない。
 */
#include "uroa_rpc.generated.h"

namespace uroa {
    namespace rpc {
        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out パケット全長以上の出力領域。
         * @param packet 呼出元が確定した幅・固定値の値。
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
         * @param packet 呼出元が確定した幅・固定値の値。
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
         * @brief 全長一致時だけ復号する。版等の不正値も応答照合用に保持する。
         * @param data 入力バイト列。
         * @param length 入力のバイト数。
         * @return nullptrまたは長さ不正なら全フィールド0。
         */
        CommandPacket decodeCommandPacket(const uint8_t *data, size_t length) {
            CommandPacket packet{};
            if (data == nullptr || length != CommandPacketLayout::ByteLength) {
                return packet;
            }
            packet.version = data[CommandPacketLayout::version];
            packet.opcode = data[CommandPacketLayout::opcode];
            packet.sequence = static_cast<uint16_t>(
                (static_cast<uint32_t>(data[CommandPacketLayout::sequence])) |
                (static_cast<uint32_t>(data[CommandPacketLayout::sequence + 1]) << 8));
            packet.channel = data[CommandPacketLayout::channel];
            packet.reserved = data[CommandPacketLayout::reserved];
            packet.value = static_cast<uint16_t>(
                (static_cast<uint32_t>(data[CommandPacketLayout::value])) |
                (static_cast<uint32_t>(data[CommandPacketLayout::value + 1]) << 8));
            packet.duration = static_cast<uint16_t>(
                (static_cast<uint32_t>(data[CommandPacketLayout::duration])) |
                (static_cast<uint32_t>(data[CommandPacketLayout::duration + 1]) << 8));
            return packet;
        }

        /**
         * @brief 生成元の固定値・enum・下限を検査する。
         * @param packet 復号した値。
         * @return 通常の形式が正しければtrue。
         */
        bool validateCommandPacket(const CommandPacket &packet) {
            return packet.version == 1 && (packet.opcode >= 1 && packet.opcode <= 11) && packet.reserved == 0;
        }

        /**
         * @brief 共通Statusの固定値・CHごとの値を設定する。
         * @param code 受付結果。
         * @param sequence 応答対象。
         * @param flags 機器状態。
         * @param channel CHまたは全体。
         * @param current 現在の指令値。
         * @param target 目標値。
         * @param remaining 残り時間ms。
         * @return CH条件を適用したStatus。
         */
        StatusPacket makeStatusPacket(uint8_t code, uint16_t sequence, uint8_t flags, uint8_t channel,
            uint16_t current, uint16_t target, uint16_t remaining) {
            StatusPacket packet{};
            packet.version = StatusPacketLayout::versionValue;
            packet.code = code;
            packet.sequence = sequence;
            packet.flags = flags;
            packet.channel = channel;
            if (channel < StatusPacketLayout::ChannelCount) {
                packet.current = current;
                packet.target = target;
                packet.remaining = remaining;
            }
            return packet;
        }

    }
}
