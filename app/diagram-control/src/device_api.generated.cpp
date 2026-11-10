/** @file device_api.generated.cpp
 * @brief device.pyiから自動生成した型検査・値スロット変換・単一ID登録表。直接編集しない。
 */
#include "device_api.generated.h"

namespace device_api {
    /** @brief 派生バックエンドを破棄する。 */
    Backend::~Backend() = default;

    namespace {
        /** @brief Servo_constructの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke1(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            int32_t arg_channel{};
            int32_t arg_mode{};
            if (call.arguments[0].type != Type::Integer) {
                return Error::Type;
            }
            arg_channel = call.arguments[0].integer;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_mode = call.arguments[1].integer;
            uint32_t result{};
            const Error status = static_cast<Backend *>(call.context)->Servo_construct(arg_channel, arg_mode, result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Object;
            call.results[0].classId = 3;
            call.results[0].address = result;
            return Error::None;
        }

        /** @brief Servo_enableの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke2(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 3) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            const Error status = static_cast<Backend *>(call.context)->Servo_enable(receiver);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Servo_disableの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke3(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 3) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            const Error status = static_cast<Backend *>(call.context)->Servo_disable(receiver);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Servo_set_angleの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke4(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            float arg_angle{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 3) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Float) {
                return Error::Type;
            }
            arg_angle = call.arguments[1].real;
            const Error status = static_cast<Backend *>(call.context)->Servo_set_angle(receiver, arg_angle);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Servo_set_speedの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke5(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            float arg_speed{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 3) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Float) {
                return Error::Type;
            }
            arg_speed = call.arguments[1].real;
            const Error status = static_cast<Backend *>(call.context)->Servo_set_speed(receiver, arg_speed);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief IMU_constructの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke6(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 0 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t result{};
            const Error status = static_cast<Backend *>(call.context)->IMU_construct(result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Object;
            call.results[0].classId = 4;
            call.results[0].address = result;
            return Error::None;
        }

        /** @brief IMU_get_valueの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke7(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 6) {
                return Error::Operand;
            }
            uint32_t receiver{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 4) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            IMUValue result{};
            const Error status = static_cast<Backend *>(call.context)->IMU_get_value(receiver, result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Float;
            call.results[0].real = result.acc.x;
            call.results[1].type = Type::Float;
            call.results[1].real = result.acc.y;
            call.results[2].type = Type::Float;
            call.results[2].real = result.acc.z;
            call.results[3].type = Type::Float;
            call.results[3].real = result.gyro.x;
            call.results[4].type = Type::Float;
            call.results[4].real = result.gyro.y;
            call.results[5].type = Type::Float;
            call.results[5].real = result.gyro.z;
            return Error::None;
        }

        /** @brief UltrasonicDistanceSensor_constructの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke8(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 0 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t result{};
            const Error status = static_cast<Backend *>(call.context)->UltrasonicDistanceSensor_construct(result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Object;
            call.results[0].classId = 5;
            call.results[0].address = result;
            return Error::None;
        }

        /** @brief UltrasonicDistanceSensor_get_distanceの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke9(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 5) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            float result{};
            const Error status = static_cast<Backend *>(call.context)->UltrasonicDistanceSensor_get_distance(receiver, result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Float;
            call.results[0].real = result;
            return Error::None;
        }

        /** @brief Display_constructの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke10(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 0 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t result{};
            const Error status = static_cast<Backend *>(call.context)->Display_construct(result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Object;
            call.results[0].classId = 6;
            call.results[0].address = result;
            return Error::None;
        }

        /** @brief Display_set_text_colorの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke11(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_color{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_color = call.arguments[1].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_set_text_color(receiver, arg_color);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_set_cursorの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke12(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 3 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_x{};
            int32_t arg_y{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_x = call.arguments[1].integer;
            if (call.arguments[2].type != Type::Integer) {
                return Error::Type;
            }
            arg_y = call.arguments[2].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_set_cursor(receiver, arg_x, arg_y);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_printの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke13(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            StringView arg_text{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (!call.vm.stringBytes(call.arguments[1], arg_text.data, arg_text.length)) {
                return Error::Type;
            }
            const Error status = static_cast<Backend *>(call.context)->Display_print(receiver, arg_text);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_draw_lineの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke14(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 6 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_start_x{};
            int32_t arg_start_y{};
            int32_t arg_end_x{};
            int32_t arg_end_y{};
            int32_t arg_color{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_start_x = call.arguments[1].integer;
            if (call.arguments[2].type != Type::Integer) {
                return Error::Type;
            }
            arg_start_y = call.arguments[2].integer;
            if (call.arguments[3].type != Type::Integer) {
                return Error::Type;
            }
            arg_end_x = call.arguments[3].integer;
            if (call.arguments[4].type != Type::Integer) {
                return Error::Type;
            }
            arg_end_y = call.arguments[4].integer;
            if (call.arguments[5].type != Type::Integer) {
                return Error::Type;
            }
            arg_color = call.arguments[5].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_draw_line(receiver, arg_start_x, arg_start_y, arg_end_x, arg_end_y, arg_color);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_draw_rectの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke15(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 6 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_left{};
            int32_t arg_top{};
            int32_t arg_width{};
            int32_t arg_height{};
            int32_t arg_color{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_left = call.arguments[1].integer;
            if (call.arguments[2].type != Type::Integer) {
                return Error::Type;
            }
            arg_top = call.arguments[2].integer;
            if (call.arguments[3].type != Type::Integer) {
                return Error::Type;
            }
            arg_width = call.arguments[3].integer;
            if (call.arguments[4].type != Type::Integer) {
                return Error::Type;
            }
            arg_height = call.arguments[4].integer;
            if (call.arguments[5].type != Type::Integer) {
                return Error::Type;
            }
            arg_color = call.arguments[5].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_draw_rect(receiver, arg_left, arg_top, arg_width, arg_height, arg_color);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_draw_circleの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke16(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 5 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_center_x{};
            int32_t arg_center_y{};
            int32_t arg_radius{};
            int32_t arg_color{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_center_x = call.arguments[1].integer;
            if (call.arguments[2].type != Type::Integer) {
                return Error::Type;
            }
            arg_center_y = call.arguments[2].integer;
            if (call.arguments[3].type != Type::Integer) {
                return Error::Type;
            }
            arg_radius = call.arguments[3].integer;
            if (call.arguments[4].type != Type::Integer) {
                return Error::Type;
            }
            arg_color = call.arguments[4].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_draw_circle(receiver, arg_center_x, arg_center_y, arg_radius, arg_color);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_fill_circleの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke17(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 5 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            int32_t arg_center_x{};
            int32_t arg_center_y{};
            int32_t arg_radius{};
            int32_t arg_color{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            if (call.arguments[1].type != Type::Integer) {
                return Error::Type;
            }
            arg_center_x = call.arguments[1].integer;
            if (call.arguments[2].type != Type::Integer) {
                return Error::Type;
            }
            arg_center_y = call.arguments[2].integer;
            if (call.arguments[3].type != Type::Integer) {
                return Error::Type;
            }
            arg_radius = call.arguments[3].integer;
            if (call.arguments[4].type != Type::Integer) {
                return Error::Type;
            }
            arg_color = call.arguments[4].integer;
            const Error status = static_cast<Backend *>(call.context)->Display_fill_circle(receiver, arg_center_x, arg_center_y, arg_radius, arg_color);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief Display_displayの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke18(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            uint32_t receiver{};
            if (call.arguments[0].type != Type::Object || call.arguments[0].classId != 6) {
                return Error::Type;
            }
            receiver = call.arguments[0].address;
            const Error status = static_cast<Backend *>(call.context)->Display_display(receiver);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief function_printの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke19(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            StringView arg_text{};
            if (!call.vm.stringBytes(call.arguments[0], arg_text.data, arg_text.length)) {
                return Error::Type;
            }
            const Error status = static_cast<Backend *>(call.context)->function_print(arg_text);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief function_sleepの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke20(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            float arg_seconds{};
            if (call.arguments[0].type != Type::Float) {
                return Error::Type;
            }
            arg_seconds = call.arguments[0].real;
            const Error status = static_cast<Backend *>(call.context)->function_sleep(arg_seconds);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::None;
            return Error::None;
        }

        /** @brief function_timeの型を検査し、機器操作へ渡す。@param call VMの入出力領域。@return 実行結果。 */
        BytecodeVM::Error invoke21(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 0 || call.resultSlots != 1) {
                return Error::Operand;
            }
            float result{};
            const Error status = static_cast<Backend *>(call.context)->function_time(result);
            if (status != Error::None) {
                return status;
            }
            call.results[0].type = Type::Float;
            call.results[0].real = result;
            return Error::None;
        }

        // JSONと同一のABIハッシュ。シグネチャ・ID・フィールド配置の差をVM開始時に検出する。
        const uint8_t abiHash[32] = {
            0x10, 0xCE, 0x8A, 0x16, 0x6F, 0x34, 0xA6, 0xBA, 0x8C, 0xB1, 0xC2, 0xE7, 0x17, 0x24, 0x5A, 0x4A, 0xAD, 0xC2, 0xD8, 0x90, 0xB5, 0xC7, 0x7E, 0x2F, 0xE4, 0xAF, 0x7F, 0xE6, 0xCB, 0x0C, 0x88, 0xF1
        };
        // nativeIdの昇順。引数と戻り値の幅はrecord展開後のスロット数。
        const BytecodeVM::NativeEntry entries[] = {
            {1, 2, 1, invoke1},
            {2, 1, 1, invoke2},
            {3, 1, 1, invoke3},
            {4, 2, 1, invoke4},
            {5, 2, 1, invoke5},
            {6, 0, 1, invoke6},
            {7, 1, 6, invoke7},
            {8, 0, 1, invoke8},
            {9, 1, 1, invoke9},
            {10, 0, 1, invoke10},
            {11, 2, 1, invoke11},
            {12, 3, 1, invoke12},
            {13, 2, 1, invoke13},
            {14, 6, 1, invoke14},
            {15, 6, 1, invoke15},
            {16, 5, 1, invoke16},
            {17, 5, 1, invoke17},
            {18, 1, 1, invoke18},
            {19, 1, 1, invoke19},
            {20, 1, 1, invoke20},
            {21, 0, 1, invoke21},
        };

        /** @brief VM停止時に機器の出力とハンドルを解放する。@param context 登録バックエンド。 */
        void resetBackend(void *context) {
            static_cast<Backend *>(context)->reset();
        }
    }

    /** @brief VM登録表を構築する。@param backend 登録中保持する機器操作。@return 静的なID表を参照する登録情報。 */
    BytecodeVM::NativeApi makeNativeApi(Backend &backend) {
        return {abiHash, entries, 21, &backend, resetBackend};
    }
}
