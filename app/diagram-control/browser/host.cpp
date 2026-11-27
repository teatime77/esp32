/**
 * @file host.cpp
 * @brief 共通C++ VMをEmscriptenから呼ぶブラウザ用ホスト。
 * @details version 8の集合照合と同期CALL_KERNELを接続する。計算用Wasmは同じWorkerにある。
 * descriptorは引数ごとにdouble4個のtag・scalar・VM内アドレス・要素数。JS側がコピーを担当する。
 */
#include "bytecode_vm.h"
#include "bytecode_format.h"
#include <emscripten.h>
#include <vector>
#include <cstring>

namespace {
    /**
     * @brief browser-abi.tsと共有する数値引数の種別。
     */
    enum KernelTag : uint8_t {
        Integer = 1,
        Float = 2,
        Input = 3,
        Output = 4
    };
    BytecodeVM vm; ///< Workerが単独で操作する共通VM。
    uint8_t kernelHash[32]{}; ///< 開始前に接続した集合指紋。
    std::vector<std::vector<uint8_t>> signatures; ///< IDで引く引数型列。0は未使用。
    double result[4]{}; ///< 呼出し直後だけ有効な大域値の読出し領域。

    /**
     * @brief 登録したIDのVM引数数を取得する。
     * @param id 非0 ID。
     * @return 未登録ならUINT16_MAX。
     */
    uint16_t argumentCount(uint16_t id) {
        return id == 0 || id >= signatures.size() ? UINT16_MAX : uint16_t(signatures[id].size());
    }

    /**
     * @brief 同じWorkerの計算用Wasmへ一度だけ接続する。
     * @param id カーネルID。
     * @param n 要素数。
     * @param descriptors double4個ずつの引数記述子。
     * @param count 明示引数数。
     * @return VMのError番号。
     */
    EM_JS(int, invokeJavascript, (uint16_t id, int32_t n, const double *descriptors, uint16_t count), {
        try {
            return Module.kernelInvoke(id, n, descriptors, count);
        } catch (error) {
            Module.kernelError = String(error);
            return 16;
        }
    });

    /**
     * @brief 接続形式・型・共有を検査してから、値を検査せず同期計算する。
     * @param machine 呼出し元VM。
     * @param id 登録済みID。
     * @param values n、明示引数の順のVM値。
     * @param count nを含む値の数。
     * @return VMエラーまたはNone。
     */
    BytecodeVM::Error invoke(BytecodeVM &machine, uint16_t id, const BytecodeVM::Value *values, uint16_t count) {
        using Error = BytecodeVM::Error;
        using Type = BytecodeVM::Type;
        const auto &types = signatures[id];
        if (count != types.size() + 1) {
            return Error::Operand;
        }
        std::vector<double> arguments(types.size() * 4);
        for (size_t index = 0; index < types.size(); ++index) {
            const auto &value = values[index + 1];
            double *argument = arguments.data() + index * 4;
            argument[0] = types[index];
            if (types[index] == Integer || types[index] == Float) {
                if (value.type != Type::Integer && (types[index] != Float || value.type != Type::Float)) {
                    return Error::Type;
                }
                argument[1] = value.type == Type::Float ? value.real : value.integer;
                continue;
            }
            const BytecodeVM::Array *array = nullptr;
            if (!machine.arrayData(value, array) || array->dtype != BytecodeVM::Array::Float32 || array->rank != 1) {
                return Error::ArrayShape;
            }
            for (size_t previous = 0; previous < index; ++previous) {
                if ((types[index] == Output || types[previous] == Output) && values[previous + 1].type == Type::HeapReference &&
                    value.reference == values[previous + 1].reference) {
                    return Error::Operand;
                }
            }
            argument[2] = uintptr_t(array + 1);
            argument[3] = array->elements;
        }
        if (values[0].integer == 0) {
            return Error::None;
        }
        return Error(invokeJavascript(id, values[0].integer, arguments.data(), uint16_t(types.size())));
    }

    const BytecodeVM::KernelApi kernelApi{kernelHash, argumentCount, invoke}; ///< Worker内の借用登録表。
}

extern "C" {
    /**
     * @brief 停止して集合情報を置き換える。hashは32バイト。
     * @param hash 呼出し中に読み取れる集合指紋。
     */
    void browser_kernel_hash(const uint8_t *hash) {
        vm.stop();
        signatures.clear();
        signatures.resize(1);
        std::memcpy(kernelHash, hash, 32);
        vm.bindKernelApi(&kernelApi);
    }

    /**
     * @brief ID順に明示引数の型列を登録する。型は1=int・2=float・3=input・4=output。
     * @param id 次に登録する非0 ID。
     * @param types count個の型タグ。
     * @param count nを除く引数数。
     * @return 登録成功なら1。
     */
    int browser_register_kernel(uint16_t id, const uint8_t *types, uint16_t count) {
        if (id != signatures.size() || count >= BytecodeVM::MaxStack) {
            return 0;
        }
        for (uint16_t index = 0; index < count; ++index) {
            if (types[index] < Integer || types[index] > Output) {
                return 0;
            }
        }
        signatures.emplace_back(types, types + count);
        return 1;
    }

    /**
     * @brief 借用するイメージの配置・命令境界・登録IDを確認して開始する。
     * @param data 停止まで有効なイメージ。
     * @param length バイト長。
     * @return 開始成功なら1。
     */
    int browser_load(const uint8_t *data, size_t length) {
        BytecodeFormat::Image image;
        if (!BytecodeFormat::parseImage(data, length, image)) {
            return 0;
        }
        for (uint32_t pc = 0; pc < image.codeLength;) {
            const uint8_t opcode = image.code[pc];
            const uint32_t size = BytecodeFormat::instructionLength(opcode);
            if (size == 0 || size > image.codeLength - pc) {
                return 0;
            }
            if (opcode == BytecodeFormat::Opcode::CALL_KERNEL) {
                const uint16_t id = image.code[pc + 1] | (uint16_t(image.code[pc + 2]) << 8);
                if (!image.kernelHash || argumentCount(id) == UINT16_MAX) {
                    return 0;
                }
            }
            pc += size;
        }
        return vm.start(data, length);
    }

    /**
     * @brief 指定命令数まで実行する。カーネル復帰時にはtickも終了する。
     * @param budget 命令数の予算。
     */
    void browser_tick(uint32_t budget) {
        vm.tick(budget);
    }
    /**
     * @brief Workerが停止要求を受け取った時点でVMを停止する。
     */
    void browser_stop() {
        vm.stop();
    }
    /**
     * @brief VM状態を取得する。
     * @return Stateの番号。
     */
    int browser_state() {
        return int(vm.state);
    }
    /**
     * @brief VMエラー番号を取得する。
     * @return Errorの番号。
     */
    int browser_error() {
        return int(vm.error);
    }
    /**
     * @brief 最後の実行位置を取得する。
     * @return エラー時はerrorPc、それ以外は次のPC。
     */
    uint32_t browser_pc() {
        return vm.state == BytecodeVM::State::Error ? vm.errorPc : vm.pc;
    }
    /**
     * @brief VM命令数を取得する。C内部の計算は含めない。
     * @return 実行したVM命令数。
     */
    uint32_t browser_instructions() {
        return vm.instructions;
    }
    /**
     * @brief 大域スロット数を取得する。
     * @return 一時領域を含む大域スロット数。
     */
    uint32_t browser_global_count() {
        return vm.globalCount;
    }
    /**
     * @brief 大域値をdouble4個のtag・値・アドレス・長さで借用する。配列は100=float32、101=uint8。
     * @param slot 大域スロット位置。
     * @return 次の呼出しまで有効な記述子。内部参照を使う間はtick・GC・stopを行わない。
     */
    const double *browser_value(uint32_t slot) {
        std::memset(result, 0, sizeof result);
        if (slot >= vm.globalCount) {
            return result;
        }
        const auto &value = vm.globals[slot];
        result[0] = int(value.type);
        if (value.type == BytecodeVM::Type::Integer) {
            result[1] = value.integer;
        }
        else if (value.type == BytecodeVM::Type::Float) {
            result[1] = value.real;
        }
        else if (value.type == BytecodeVM::Type::String) {
            const char *data = nullptr;
            size_t length = 0;
            if (vm.stringBytes(value, data, length)) {
                result[2] = uintptr_t(data);
                result[3] = length;
            }
        }
        else {
            const BytecodeVM::Array *array = nullptr;
            if (vm.arrayData(value, array) && array->dtype != BytecodeVM::Array::Object) {
                result[0] = array->dtype == BytecodeVM::Array::Float32 ? 100 : 101;
                result[2] = uintptr_t(array + 1);
                result[3] = array->elements;
            }
        }
        return result;
    }
}
