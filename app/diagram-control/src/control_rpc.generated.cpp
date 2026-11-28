/**
 * @file control_rpc.generated.cpp
 * @brief device.protoの受理条件・機能振分け・応答作成。手動編集しない。
 */
#include "control_rpc.generated.h"
#include <math.h>

namespace uroa {
    namespace rpc {
        /**
         * @brief 状態をwire値へ変換し、生成したStatusを現在の窓口へ通知する。
         * @param service 状態取得と通知窓口。
         * @param code 結果。
         * @param sequence 応答対象。
         * @param channel CHまたは全体。
         */
        void publishControlStatus(const ControlReplyService &service, uint8_t code, uint16_t sequence, uint8_t channel) {
            const ControlStatusState state = service.snapshot(channel);
            uint8_t flags = state.flags;
            uint16_t current = state.current;
            uint16_t target = state.target;
            if (state.speedMode) {
                flags |= StatusPacketLayout::SpeedFlag;
                current = static_cast<uint16_t>(static_cast<int16_t>(lroundf(state.speedPercent * StatusPacketLayout::SpeedScale)));
                target = current;
            }
            const StatusPacket packet = makeStatusPacket(code, sequence, flags, channel, current, target, state.remaining);
            uint8_t bytes[StatusPacketLayout::ByteLength];
            encodeStatusPacket(bytes, packet);
            service.publish(bytes, sizeof bytes);
        }

        /**
         * @brief 受理条件とopcode対応から機能を呼び、共通のStatusを返す。
         * @param command 復号済みの値。
         * @param context 呼出元の世代と停止優先状態。
         * @param services 型付きの機能表。
         */
        void dispatchControl(const CommandPacket &command, const ControlRpcContext &context, const ControlServices &services) {
            if (!validateCommandPacket(command)) {
                publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_FORMAT), command.sequence, 255);
                return;
            }
            services.reply.accepted();
            switch (static_cast<CommandOpcode>(command.opcode)) {
                case CommandOpcode::INFO: {
                    const StatusCode code = StatusCode::CODE_OK;
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::PING: {
                    const StatusCode code = StatusCode::CODE_OK;
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::ARM: {
                    if (command.channel >= StatusPacketLayout::ChannelCount) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.duration > 10000) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, command.channel);
                        return;
                    }
                    const StatusCode code = services.servo.armAngle(command.channel, command.value / 100.0f);
                    if (code == StatusCode::CODE_IO_ERROR) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(code), 0, command.channel);
                    }
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::ARM_SPEED: {
                    if (command.channel >= StatusPacketLayout::ChannelCount) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.value != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, command.channel);
                        return;
                    }
                    if (command.duration != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, command.channel);
                        return;
                    }
                    const StatusCode code = services.servo.armSpeed(command.channel);
                    if (code == StatusCode::CODE_IO_ERROR) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(code), 0, command.channel);
                    }
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::MOVE: {
                    if (command.channel >= StatusPacketLayout::ChannelCount) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    const StatusCode code = services.servo.move(command.channel, command.value / 100.0f, command.duration);
                    if (code == StatusCode::CODE_IO_ERROR) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(code), 0, command.channel);
                    }
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::SET_SPEED: {
                    if (command.channel >= StatusPacketLayout::ChannelCount) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.duration != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, command.channel);
                        return;
                    }
                    const int32_t signedValue = command.value >= 32768u
                        ? static_cast<int32_t>(command.value) - 65536 : command.value;
                    const StatusCode code = services.servo.speed(command.channel, signedValue / 100.0f);
                    if (code == StatusCode::CODE_IO_ERROR) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(code), 0, command.channel);
                    }
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::DISABLE: {
                    if (command.channel >= StatusPacketLayout::ChannelCount) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    const StatusCode code = services.servo.disable(command.channel);
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, command.channel);
                    return;
                }
                case CommandOpcode::STOP: {
                    const StatusCode code = services.servo.stop();
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, 255);
                    return;
                }
                case CommandOpcode::VM_START: {
                    if (command.channel != 255) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.value != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.duration != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    const StatusCode code = services.program.start(context);
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, 255);
                    return;
                }
                case CommandOpcode::VM_STOP: {
                    if (command.channel != 255) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.value != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    if (command.duration != 0) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    const StatusCode code = services.program.stop(context);
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, 255);
                    return;
                }
                case CommandOpcode::STREAM: {
                    if (command.value != 0 && command.value != 5 && command.value != 10 && command.value != 20) {
                        publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_RANGE), command.sequence, 255);
                        return;
                    }
                    const StatusCode code = services.control.stream(command.value);
                    publishControlStatus(services.reply, static_cast<uint8_t>(code), command.sequence, 255);
                    return;
                }
                default:
                    publishControlStatus(services.reply, static_cast<uint8_t>(StatusCode::CODE_FORMAT), command.sequence, 255);
                    return;
            }
        }
    }
}
