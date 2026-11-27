/**
 * @file host.cpp
 * @brief 共通C++ VMをEmscriptenから呼ぶブラウザ用ホスト。
 * @details version 8の集合照合と同期CALL_KERNELを接続する。計算用Wasmは同じWorkerにある。
 * descriptorは引数ごとにdouble4個のtag・scalar・VM内アドレス・要素数。JS側がコピーを担当する。
 * printは解析済みAPIのID・指紋で専用NativeApiへ登録し、長さ付きUTF-8をWorkerへ同期的に渡す。
 * ブラウザRPCの公開表はbrowser-abi.ts/Workerから受け、selfはVMの大域スロットで保持する。
 * 関数のRETで止め、結果をJSへコピーした後にreleaseする。BLE/Wi-Fiの配送は扱わない。
 */
#include "bytecode_vm.h"
#include "bytecode_format.h"
#include <emscripten.h>
#include <vector>
#include <cstring>
#include <cmath>

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
    uint8_t nativeHash[32]{}; ///< ブラウザのprint登録に使う、解析済みAPIの指紋。
    BytecodeFormat::Image loadedImage{}; ///< VMと同じイメージの借用ビュー。stop時に解除する。

    /**
     * @brief 公開メソッドの数値登録情報。GC本体のポインターは保持しない。
     */
    struct RpcMethod {
        uint16_t object; ///< 公開オブジェクトID。
        uint16_t method; ///< オブジェクト内のメソッドID。
        uint16_t slot; ///< selfを保持する大域スロット。
        uint16_t classId; ///< selfの本体型ID。
        uint16_t entry; ///< 関数表と照合済みの入口PC。
        uint8_t result; ///< 戻り値のVMタグ。
        std::vector<uint8_t> arguments; ///< 明示引数のVMタグ列。
    };

    std::vector<RpcMethod> rpcMethods; ///< 準備完了前に登録し、停止まで保持する公開表。
    uint8_t rpcResultType = 0; ///< 現在の要求で期待する戻り値タグ。releaseで消去する。

    /**
     * @brief 第2段階で外部へ渡せるスカラーのVMタグか調べる。
     * @param tag 型タグ。
     * @return int・float・None・文字列ならtrue。
     */
    bool rpcScalar(uint8_t tag) {
        return tag == uint8_t(BytecodeVM::Type::Integer) || tag == uint8_t(BytecodeVM::Type::Float) ||
            tag == uint8_t(BytecodeVM::Type::None) || tag == uint8_t(BytecodeVM::Type::String);
    }

    /**
     * @brief 借用したUTF-8を呼出し中にJS文字列へコピーする。
     * @param text 長さ付き文字列の先頭。
     * @param length 終端NULを含まないUTF-8バイト数。
     * @return 成功ならNone、出力処理の例外はNativeFailure。
     */
    EM_JS(int, emitPrint, (const char *text, size_t length), {
        try {
            Module.pythonPrint(text, length);
            return 0;
        } catch (error) {
            return 16;
        }
    });

    /**
     * @brief printの文字列1引数を出力し、Noneを返す。
     * @param call 同期呼出し中だけ有効な入出力領域。
     * @return 引数・文字列を検査した結果。
     * @details 改行の付加とUIへの転送はWorkerが担当する。VMの文字列参照を保持しない。
     */
    BytecodeVM::Error invokePrint(BytecodeVM::NativeCall &call) {
        if (call.argumentSlots != 1 || call.resultSlots != 1) {
            return BytecodeVM::Error::Operand;
        }
        const char *text = nullptr;
        size_t length = 0;
        if (!call.vm.stringBytes(call.arguments[0], text, length)) {
            return BytecodeVM::Error::Type;
        }
        const auto error = BytecodeVM::Error(emitPrint(text, length));
        if (error == BytecodeVM::Error::None) {
            call.results[0].type = BytecodeVM::Type::None;
        }
        return error;
    }

    BytecodeVM::NativeEntry printEntry{0, 1, 1, invokePrint}; ///< 開始前に解析済みのprint IDを設定する。
    const BytecodeVM::NativeApi nativeApi{nativeHash, &printEntry, 1, nullptr, nullptr}; ///< printだけを公開するブラウザ用表。

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
     * @brief 解析済みAPIのprintを停止中に登録する。IDを機器用生成物から複製しない。
     * @param id 同じ解析から取得したprintのnativeId。
     * @param hash APIのSHA-256。イメージの指紋とはstartと呼出し時に照合する。
     * @return 登録成功なら1。
     */
    int browser_bind_print(uint16_t id, const uint8_t *hash) {
        if (id == 0 || !hash || vm.state == BytecodeVM::State::Running || vm.state == BytecodeVM::State::Paused) {
            return 0;
        }
        printEntry.id = id;
        std::memcpy(nativeHash, hash, 32);
        return vm.bindNativeApi(&nativeApi);
    }

    /**
     * @brief 停止して集合情報を置き換える。hashは32バイト。
     * @param hash 呼出し中に読み取れる集合指紋。
     */
    void browser_kernel_hash(const uint8_t *hash) {
        vm.stop();
        loadedImage = {};
        rpcMethods.clear();
        rpcResultType = 0;
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
        if (!vm.start(data, length)) {
            return 0;
        }
        loadedImage = image;
        return 1;
    }

    /**
     * @brief 初期化終了後、公開署名と関数表・selfを照合してメソッドを登録する。
     * @param object 公開オブジェクトID。
     * @param method 公開メソッドID。
     * @param slot selfの大域スロット。
     * @param classId selfの型ID。
     * @param entry 関数の入口PC。
     * @param tags 明示引数と戻り値の順のcount+1個のVMタグ。
     * @param count 明示引数数。
     * @return 登録成功なら1。不正な署名・重複・初期化前は0。
     */
    int browser_rpc_register(uint16_t object, uint16_t method, uint16_t slot, uint16_t classId, uint16_t entry,
        const uint8_t *tags, uint16_t count) {
        if (vm.state != BytecodeVM::State::Halted || !loadedImage.functions || object == 0 || method == 0 ||
            slot >= loadedImage.globalRootCount || !tags || uint32_t(count) + 3 > BytecodeVM::MaxStack) {
            return 0;
        }
        for (const RpcMethod &registered : rpcMethods) {
            if (registered.object == object && registered.method == method) {
                return 0;
            }
        }
        const auto &self = vm.globals[slot];
        if (self.type != BytecodeVM::Type::HeapReference || self.classId != classId) {
            return 0;
        }
        for (uint32_t index = 0; index <= count; ++index) {
            if (!rpcScalar(tags[index])) {
                return 0;
            }
        }
        for (uint32_t index = 0; index < loadedImage.functionCount; ++index) {
            const uint8_t *record = loadedImage.functions + index * BytecodeFormat::FunctionRecordLength;
            const uint16_t pc = record[0] | (uint16_t(record[1]) << 8);
            if (pc != entry) {
                continue;
            }
            const uint16_t arguments = record[4] | (uint16_t(record[5]) << 8);
            const uint16_t results = record[8] | (uint16_t(record[9]) << 8);
            if (arguments != uint32_t(count) + 1 || results != 1) {
                return 0;
            }
            rpcMethods.push_back(
                {object, method, slot, classId, entry, tags[count], std::vector<uint8_t>(tags, tags + count)});
            return 1;
        }
        return 0;
    }

    /**
     * @brief 登録済みメソッドへスカラー引数をコピーし、実行を開始する。
     * @param object 公開オブジェクトID。
     * @param method 公開メソッドID。
     * @param descriptors tag・scalar・address・lengthのdouble4個を引数ごとに並べた領域。
     * @param count 明示引数数。
     * @return 開始成功なら1。不正な対象・型・状態は0。
     * @details C++のValue列はこの同期呼出し中だけ所有し、文字列確保中にGCを行わない。
     */
    int browser_rpc_begin(uint16_t object, uint16_t method, const double *descriptors, uint16_t count) {
        if (vm.state != BytecodeVM::State::Halted || rpcResultType != 0 || (count != 0 && !descriptors)) {
            return 0;
        }
        for (const RpcMethod &entry : rpcMethods) {
            if (entry.object != object || entry.method != method || entry.arguments.size() != count) {
                continue;
            }
            // 全引数の型・値を確保前に検査する。不正な要求で文字列プールを消費しない。
            for (uint16_t index = 0; index < count; ++index) {
                const double *argument = descriptors + index * 4;
                const auto tag = BytecodeVM::Type(entry.arguments[index]);
                if (argument[0] != entry.arguments[index] ||
                    (tag == BytecodeVM::Type::Integer &&
                        (!std::isfinite(argument[1]) || argument[1] < INT32_MIN || argument[1] > INT32_MAX ||
                            std::trunc(argument[1]) != argument[1])) ||
                    (tag == BytecodeVM::Type::Float && !std::isfinite(float(argument[1]))) ||
                    (tag == BytecodeVM::Type::String &&
                        (argument[3] < 0 || argument[3] > BytecodeVM::MaxStringLength ||
                            std::trunc(argument[3]) != argument[3]))) {
                    return 0;
                }
            }
            std::vector<BytecodeVM::Value> arguments(count);
            for (uint16_t index = 0; index < count; ++index) {
                const double *argument = descriptors + index * 4;
                auto &value = arguments[index];
                value.type = BytecodeVM::Type(entry.arguments[index]);
                if (value.type == BytecodeVM::Type::Integer) {
                    value.integer = int32_t(argument[1]);
                }
                else if (value.type == BytecodeVM::Type::Float) {
                    value.real = float(argument[1]);
                }
                else if (value.type == BytecodeVM::Type::String &&
                    !vm.copyBrowserText(
                        reinterpret_cast<const char *>(uintptr_t(argument[2])), size_t(argument[3]), value)) {
                    return 0;
                }
            }
            if (!vm.beginBrowserCall(entry.entry, entry.slot, entry.classId, arguments.data(), count)) {
                return 0;
            }
            rpcResultType = entry.result;
            return 1;
        }
        return 0;
    }

    /**
     * @brief RPCの戻り値を借用記述子へ変換する。release前にJSへコピーする。
     * @return double4個。結果の型不一致・未完了ならnull。
     */
    const double *browser_rpc_result() {
        const auto *value = vm.browserCallResult();
        if (!value || uint8_t(value->type) != rpcResultType) {
            return nullptr;
        }
        std::memset(result, 0, sizeof result);
        result[0] = int(value->type);
        if (value->type == BytecodeVM::Type::Integer) {
            result[1] = value->integer;
        }
        else if (value->type == BytecodeVM::Type::Float) {
            result[1] = value->real;
        }
        else if (value->type == BytecodeVM::Type::String) {
            const char *data = nullptr;
            size_t length = 0;
            if (!vm.stringBytes(*value, data, length)) {
                return nullptr;
            }
            result[2] = uintptr_t(data);
            result[3] = length;
        }
        return result;
    }

    /**
     * @brief 結果のコピー後に一時値を回収し、次の要求を受け付ける。
     * @return 成功なら1。
     */
    int browser_rpc_release() {
        if (!vm.releaseBrowserCall()) {
            return 0;
        }
        rpcResultType = 0;
        return 1;
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
        loadedImage = {};
        rpcMethods.clear();
        rpcResultType = 0;
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
