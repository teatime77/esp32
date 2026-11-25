/**
 * @file device_api.generated.cpp
 * @brief device.pyiから自動生成した型検査・値スロット変換・単一ID登録表。直接編集しない。
 * @details 数学関数は共通変換から直接呼び、機器操作はBackendへ渡す。有限性はVMが検査する。
 */
#include "device_api.generated.h"
#include <cmath>

namespace device_api {
    /**
     * @brief 派生バックエンドを破棄する。
     */
    Backend::~Backend() = default;

    namespace {
        /**
         * @brief float単項関数の引数を検査し、結果をVMスロットへ格納する。
         * @param call VMの入出力領域。
         * @param function float版の標準関数またはそのアダプター。
         * @return 成功はNone、スロット数不一致はOperand、型不一致はType。
         * @details
         * 数学関数の共通仕様。device.pyiの宣言からdevice_api_cpp.pyが
         * invoke_<関数名>とID表を生成し、BackendやDeviceNativeを介さず標準関数を呼ぶ。
         * PythonBlockからimportなしで呼び出す。整数の引数はコンパイラーがfloatへ変換する。
         * 引数・戻り値はVMのfloat32で、対応するC++標準関数のfloat版を使う。
         * absはstd::fabsで絶対値をfloatで返す。整数引数もfloatへ変換し、負のゼロは正のゼロになる。
         * floorは入力以下の最大の整数値、ceilは入力以上の最小の整数値をfloatで返す。
         * Python標準のmath.floor/ceilと異なりintは返さず、int32への変換・範囲制限は行わない。
         * modfはfractionalPartでstd::modfの整数部を破棄し、小数部だけをfloatで返す。
         * Python標準のmath.modfと異なりタプルは返さない。小数部は入力と同符号で、
         * modf(-1.25)は-0.25。整数部はゼロ方向へ切り捨てられ、整数入力の結果は符号付きゼロ。
         * sin・cos・tanの引数とacos・asinの戻り値の角度はラジアン。
         * acos・asinの入力範囲は[-1, 1]。範囲外の非有限な計算結果もVMがTypeとして拒否する。
         * sin・cos・tanでは度からの変換、入力角度の制限、独自の周期丸めは行わない。
         * 有限性の検査はbytecode_vm.cppのcallNative()/validValue()が呼出し前後に行う。
         * NaN・無限大の引数と非有限の計算結果はそこでTypeとなり、VMの実行を停止する。
         * tanは丸められたfloat32の角度に対して計算する。特異点付近でも独自のしきい値で
         * 拒否せず、有限の結果はそのまま返す。数学上のπ/2等でも丸めにより大きな有限値になり得る。
         * 機器・プログラムの起点には依存せず、待機・NativePendingは使わない。
         * loop()からVMを通して呼ぶ。contextは使用しない。
         */
        BytecodeVM::Error invokeFloatUnary(BytecodeVM::NativeCall &call, float (*function)(float)) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (call.argumentSlots != 1 || call.resultSlots != 1) {
                return Error::Operand;
            }
            if (call.arguments[0].type != Type::Float) {
                return Error::Type;
            }
            call.results[0].type = Type::Float;
            call.results[0].real = function(call.arguments[0].real);
            return Error::None;
        }

        /**
         * @brief float二項関数の引数を検査し、結果をVMスロットへ格納する。
         * @param call VMの入出力領域。
         * @param function float版の標準関数。
         * @return 成功はNone、スロット数不一致はOperand、型不一致はType。
         * @details
         * float32・整数引数の変換・有限性・実行タスクの共通仕様はinvokeFloatUnaryを参照する。
         * min/maxは数値2引数だけを受け、std::fmin/fmaxで小さい値/大きい値をfloatで返す。
         * 整数同士でも戻り値はfloat。可変長引数・反復可能オブジェクト・key/defaultは未対応。
         * atan2(y, x)はstd::atan2(y, x)で象限を含む角度をラジアンで返す。
         * ゼロ・符号付きゼロの扱いは標準関数に従い、独自の原点判定は行わない。
         */
        BytecodeVM::Error invokeFloatBinary(BytecodeVM::NativeCall &call, float (*function)(float, float)) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (call.argumentSlots != 2 || call.resultSlots != 1) {
                return Error::Operand;
            }
            if (call.arguments[0].type != Type::Float || call.arguments[1].type != Type::Float) {
                return Error::Type;
            }
            call.results[0].type = Type::Float;
            call.results[0].real = function(call.arguments[0].real, call.arguments[1].real);
            return Error::None;
        }

        /**
         * @brief std::modfの小数部だけを返す。仕様はinvokeFloatUnaryを参照する。
         * @param value 分解する値。@return 入力と同符号の小数部。
         */
        float fractionalPart(float value) {
            float integerPart{};
            return std::modf(value, &integerPart);
        }

        /**
         * @brief GC本体をC++のVec3へコピーする。
         * @param vm 本体を検査するVM。
         * @param value 本体参照。
         * @param result 成功時のC++値。
         * @return 型・幅が一致すればtrue。
         */
        inline bool read_Vec3(BytecodeVM &vm, const BytecodeVM::Value &value, Vec3 &result) {
            const BytecodeVM::Value *fields = nullptr;
            if (!vm.heapFields(value, 1, 3, fields)) {
                return false;
            }
            if (fields[0].type != BytecodeVM::Type::Float || !std::isfinite(fields[0].real)) {
                return false;
            }
            result.x = fields[0].real;
            if (fields[1].type != BytecodeVM::Type::Float || !std::isfinite(fields[1].real)) {
                return false;
            }
            result.y = fields[1].real;
            if (fields[2].type != BytecodeVM::Type::Float || !std::isfinite(fields[2].real)) {
                return false;
            }
            result.z = fields[2].real;
            return true;
        }

        /**
         * @brief C++のVec3を入れ子も含めてGC本体へコピーする。
         * @param vm 確保先のVM。
         * @param input バックエンドの結果。
         * @param value 成功時だけ設定する参照。
         * @return 確保と変換の成功時true。
         * @details 構築途中の親をHeapRootで保持し、子の確保中のGCから保護する。
         */
        inline bool write_Vec3(BytecodeVM &vm, const Vec3 &input, BytecodeVM::Value &value) {
            BytecodeVM::Value holder;
            BytecodeVM::HeapRoot root(vm, holder);
            if (!vm.allocateHeapValue(1, 3, 0, holder)) {
                return false;
            }
            auto *fields = static_cast<BytecodeVM::Value *>(holder.reference->data);
            if (!std::isfinite(input.x)) {
                return false;
            }
            fields[0].type = BytecodeVM::Type::Float;
            fields[0].real = input.x;
            if (!std::isfinite(input.y)) {
                return false;
            }
            fields[1].type = BytecodeVM::Type::Float;
            fields[1].real = input.y;
            if (!std::isfinite(input.z)) {
                return false;
            }
            fields[2].type = BytecodeVM::Type::Float;
            fields[2].real = input.z;
            value = holder;
            return true;
        }

        /**
         * @brief GC本体をC++のIMUValueへコピーする。
         * @param vm 本体を検査するVM。
         * @param value 本体参照。
         * @param result 成功時のC++値。
         * @return 型・幅が一致すればtrue。
         */
        inline bool read_IMUValue(BytecodeVM &vm, const BytecodeVM::Value &value, IMUValue &result) {
            const BytecodeVM::Value *fields = nullptr;
            if (!vm.heapFields(value, 2, 2, fields)) {
                return false;
            }
            if (!read_Vec3(vm, fields[0], result.acc)) {
                return false;
            }
            if (!read_Vec3(vm, fields[1], result.gyro)) {
                return false;
            }
            return true;
        }

        /**
         * @brief C++のIMUValueを入れ子も含めてGC本体へコピーする。
         * @param vm 確保先のVM。
         * @param input バックエンドの結果。
         * @param value 成功時だけ設定する参照。
         * @return 確保と変換の成功時true。
         * @details 構築途中の親をHeapRootで保持し、子の確保中のGCから保護する。
         */
        inline bool write_IMUValue(BytecodeVM &vm, const IMUValue &input, BytecodeVM::Value &value) {
            BytecodeVM::Value holder;
            BytecodeVM::HeapRoot root(vm, holder);
            if (!vm.allocateHeapValue(2, 2, 0, holder)) {
                return false;
            }
            auto *fields = static_cast<BytecodeVM::Value *>(holder.reference->data);
            if (!write_Vec3(vm, input.acc, fields[0])) {
                return false;
            }
            if (!write_Vec3(vm, input.gyro, fields[1])) {
                return false;
            }
            value = holder;
            return true;
        }

        /**
         * @brief Servo_constructの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Servo_construct(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Servo_enableの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Servo_enable(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Servo_disableの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Servo_disable(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Servo_set_angleの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Servo_set_angle(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Servo_set_speedの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Servo_set_speed(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief IMU_constructの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_IMU_construct(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief IMU_get_valueの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_IMU_get_value(BytecodeVM::NativeCall &call) {
            using Type = BytecodeVM::Type;
            using Error = BytecodeVM::Error;
            if (!call.context || call.argumentSlots != 1 || call.resultSlots != 1) {
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
            if (!write_IMUValue(call.vm, result, call.results[0])) {
                return call.vm.error == Error::None ? Error::Type : call.vm.error;
            }
            return Error::None;
        }

        /**
         * @brief UltrasonicDistanceSensor_constructの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_UltrasonicDistanceSensor_construct(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief UltrasonicDistanceSensor_get_distanceの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_UltrasonicDistanceSensor_get_distance(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_constructの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_construct(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_set_text_colorの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_set_text_color(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_set_cursorの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_set_cursor(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_printの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_print(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_draw_lineの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_draw_line(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_draw_rectの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_draw_rect(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_draw_circleの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_draw_circle(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_fill_circleの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_fill_circle(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief Display_displayの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_Display_display(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief function_printの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_print(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief function_sleepの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_sleep(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief function_timeの型を検査し、機器操作へ渡す。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_time(BytecodeVM::NativeCall &call) {
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

        /**
         * @brief std::sinを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_sin(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::sin));
        }

        /**
         * @brief std::cosを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_cos(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::cos));
        }

        /**
         * @brief std::tanを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_tan(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::tan));
        }

        /**
         * @brief std::fminを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_min(BytecodeVM::NativeCall &call) {
            return invokeFloatBinary(call, static_cast<float (*)(float, float)>(std::fmin));
        }

        /**
         * @brief std::fmaxを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_max(BytecodeVM::NativeCall &call) {
            return invokeFloatBinary(call, static_cast<float (*)(float, float)>(std::fmax));
        }

        /**
         * @brief std::floorを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_floor(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::floor));
        }

        /**
         * @brief std::ceilを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_ceil(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::ceil));
        }

        /**
         * @brief fractionalPartを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_modf(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(fractionalPart));
        }

        /**
         * @brief std::acosを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_acos(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::acos));
        }

        /**
         * @brief std::asinを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_asin(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::asin));
        }

        /**
         * @brief std::atan2を共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_atan2(BytecodeVM::NativeCall &call) {
            return invokeFloatBinary(call, static_cast<float (*)(float, float)>(std::atan2));
        }

        /**
         * @brief std::fabsを共通変換から呼ぶ。
         * @param call VMの入出力領域。
         * @return 実行結果。
         */
        BytecodeVM::Error invoke_abs(BytecodeVM::NativeCall &call) {
            return invokeFloatUnary(call, static_cast<float (*)(float)>(std::fabs));
        }

        // JSONと同一のABIハッシュ。シグネチャ・ID・フィールド配置の差をVM開始時に検出する。
        const uint8_t abiHash[32] = {
            0x7F, 0x12, 0xFE, 0x43, 0x91, 0x8D, 0x63, 0x00, 0x23, 0x4C, 0xC3, 0xFC, 0x57, 0x0C, 0xE1, 0xDE, 0x90, 0x0E, 0x72, 0x36, 0xF8, 0x09, 0x63, 0xD4, 0x59, 0x38, 0x56, 0x2C, 0x66, 0xFD, 0x93, 0xA7
        };
        // nativeIdの昇順。recordも1スロットのGC参照で受け渡す。
        const BytecodeVM::NativeEntry entries[] = {
            {1, 2, 1, invoke_Servo_construct},
            {2, 1, 1, invoke_Servo_enable},
            {3, 1, 1, invoke_Servo_disable},
            {4, 2, 1, invoke_Servo_set_angle},
            {5, 2, 1, invoke_Servo_set_speed},
            {6, 0, 1, invoke_IMU_construct},
            {7, 1, 1, invoke_IMU_get_value},
            {8, 0, 1, invoke_UltrasonicDistanceSensor_construct},
            {9, 1, 1, invoke_UltrasonicDistanceSensor_get_distance},
            {10, 0, 1, invoke_Display_construct},
            {11, 2, 1, invoke_Display_set_text_color},
            {12, 3, 1, invoke_Display_set_cursor},
            {13, 2, 1, invoke_Display_print},
            {14, 6, 1, invoke_Display_draw_line},
            {15, 6, 1, invoke_Display_draw_rect},
            {16, 5, 1, invoke_Display_draw_circle},
            {17, 5, 1, invoke_Display_fill_circle},
            {18, 1, 1, invoke_Display_display},
            {19, 1, 1, invoke_print},
            {20, 1, 1, invoke_sleep},
            {21, 0, 1, invoke_time},
            {22, 1, 1, invoke_sin},
            {23, 1, 1, invoke_cos},
            {24, 1, 1, invoke_tan},
            {25, 2, 1, invoke_min},
            {26, 2, 1, invoke_max},
            {27, 1, 1, invoke_floor},
            {28, 1, 1, invoke_ceil},
            {29, 1, 1, invoke_modf},
            {30, 1, 1, invoke_acos},
            {31, 1, 1, invoke_asin},
            {32, 2, 1, invoke_atan2},
            {33, 1, 1, invoke_abs},
        };

        /**
         * @brief VM停止時に機器の出力とハンドルを解放する。
         * @param context 登録バックエンド。
         */
        void resetBackend(void *context) {
            static_cast<Backend *>(context)->reset();
        }
    }

    /**
     * @brief VM登録表を構築する。
     * @param backend 登録中保持する機器操作。
     * @return 静的なID表を参照する登録情報。
     */
    BytecodeVM::NativeApi makeNativeApi(Backend &backend) {
        return {abiHash, entries, 33, &backend, resetBackend};
    }
}
