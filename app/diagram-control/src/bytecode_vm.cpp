/**
 * @file bytecode_vm.cpp
 * @brief 値スロット・関数フレーム・ネイティブ呼出しとGCヒープを持つPythonサブセットVM。
 *
 * @details
 * # PythonサブセットVM
 *
 * ESP32ではBytecodeReceiverで構造検証済みのversion=7を実行する。ブラウザ用の__EMSCRIPTEN__ビルドは
 * browser/host.cppからversion=7/8を受け、専用登録表とCALL_KERNELを接続する。固定配置と命令長はbytecode_format.cppを共有する。バイナリ規約は
 * uroa/diagram/ts/python/bytecode.tsのencodeBinary()の仕様コメントを参照。bytecode_vm.hは公開状態と登録API、
 * 本ファイルは実行、main.cppはloop()単位の開始・停止・実行量制御を担当する。
 * ヒープ確保・スイープはbytecode_heap.cpp、回収点の判定・根とValue列の再帰マークは本ファイルが担当する。
 * 数値NumPyの処理はnumpy_native.cpp、関数ID・引数番号はdevice_api.generated.hを共有する。
 * Snapshotの固定領域・値・文字列ヘッダーはwire.protoの生成codecへ渡す。
 * 値タグ・GC参照・文字列の列挙と寿命は本VMが管理し、ヒープ本体は送信しない。
 * テスト・Upload・実機動作はビルドとは別の確認事項とする。
 */
#include <uroa_wire.generated.h>
#include "bytecode_vm.h"
#include "bytecode_format.h"
#include "numpy_native.h"
#include <cstring>
#include <cmath>
#include <new>
#include <limits.h>
#include <cstdio>

using BytecodeFormat::Opcode;

namespace {
    /**
     * @brief 0長軸を先に調べ、乗算の上限を検査する。
     * @param shape rank個の軸長。
     * @param rank 1～4。
     * @param maximum 許可する最大要素数。
     * @param elements 成功時の積。0長軸があれば0。
     * @return 軸長と積が範囲内ならtrue。
     */
    bool arrayElements(const uint32_t *shape, uint8_t rank, size_t maximum, uint32_t &elements) {
        if (!shape || rank < 1 || rank > 4) {
            return false;
        }
        bool empty = false;
        for (uint8_t axis = 0; axis < rank; ++axis) {
            if (shape[axis] > INT32_MAX) {
                return false;
            }
            empty = empty || shape[axis] == 0;
        }
        elements = empty ? 0 : 1;
        if (!empty) {
            for (uint8_t axis = 0; axis < rank; ++axis) {
                if (elements > maximum / shape[axis]) {
                    return false;
                }
                elements *= shape[axis];
            }
        }
        return true;
    }

    /**
     * @brief little endianの32bit整数を書き込む。
     * @param data 書込み可能な4バイト。
     * @param value 書込む整数。
     */
    void write32(uint8_t *data, uint32_t value) {
        for (size_t byte = 0; byte < 4; ++byte) {
            data[byte] = uint8_t(value >> (byte * 8));
        }
    }

    /**
     * @brief 値をSnapshotの8バイトへ変換する。GC参照はclassIdだけを送り、生アドレス・本体IDを送らない。
     * @param record 出力8バイト。
     * @param value 検証済みの値。
     */
    void snapshotValue(uint8_t *record, const BytecodeVM::Value &value) {
        using Type = BytecodeVM::Type;
        uroa::wire::SnapshotValue packet{};
        packet.tag = static_cast<uint8_t>(value.type);
        uint32_t payload = 0;
        if (value.type == Type::Float) {
            std::memcpy(&payload, &value.real, sizeof payload);
        }
        else if (value.type == Type::Integer) {
            payload = uint32_t(value.integer);
        }
        else if (value.type != Type::None && value.type != Type::Uninitialized && value.type != Type::HeapReference) {
            payload = value.address;
        }
        if (value.type == Type::HeapReference || value.type == Type::Object) {
            packet.classId = value.classId;
        }
        packet.payload = payload;
        uroa::wire::writeSnapshotValue(record, packet);
    }

    /**
     * @brief Value列から動的文字列プールの使用中IDを集める。
     * @param values 読取り対象。
     * @param count スロット数。
     * @param used MaxStrings個の使用フラグ。
     */
    void useTextSlots(const BytecodeVM::Value *values, size_t count, bool *used) {
        for (size_t index = 0; index < count; ++index) {
            const BytecodeVM::Value &value = values[index];
            if (value.type == BytecodeVM::Type::String && (value.address & 0x80000000U) != 0) {
                const uint32_t id = value.address & 0x7FFFFFFFU;
                if (id < BytecodeVM::MaxStrings) {
                    used[id] = true;
                }
            }
        }
    }

    /**
     * @brief 非整列のlittle endian値を読む。
     * @param p 2バイト。
     * @return 数値。
     */
    uint16_t read16(const uint8_t *p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }
    /**
     * @brief 非整列のlittle endian値を読む。
     * @param p 4バイト。
     * @return 数値。
     */
    uint32_t read32(const uint8_t *p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }
}

/**
 * @brief 根と本体先頭のValue列から参照を再帰的にたどる。
 * @param values 根または本体のValue列。
 * @param count Value数。数値・文字列ID・機器ハンドルを本体として探索しない。
 * @details 参照構造が浅いことを前提とし、markedを先に設定して循環・共有を打ち切る。
 */
void BytecodeVM::markValues(const Value *values, uint32_t count) {
    for (uint32_t index = 0; index < count; ++index) {
        if (values[index].type == Type::HeapReference) {
            BytecodeHeap::Block *block = values[index].reference;
            if (block && !block->marked) {
                block->marked = true;
                markValues(static_cast<const Value *>(block->data), block->valueCount);
            }
        }
    }
}

/**
 * @brief コンパイラーが指定したメインループの先頭だけでGCを行う。
 * @param entry 0=モジュール、非0=モジュールから直接呼ぶ入口関数のPC。
 * @details 通常はGC_LOOP、ブラウザではRPCの結果コピー後のreleaseBrowserCall()からも呼ぶ。
 * 呼出し先フレーム・式の一時値が残る場合は回収しない。
 * 根は大域変数とメインの引数・局所変数だけ。C++の構築処理は終了しており、参照を持ち越さない。
 * 確保処理中・sleepのC++スタブ中・命令予算切れ・一時停止中には回収しない。
 */
void BytecodeVM::collectHeap(uint16_t entry) {
    if (state != State::Running || bp != 0 || sp != floor_ ||
        (entry == 0 ? depth_ != 0 : depth_ != 1 || base_ != entry)) {
        return;
    }
    uint32_t roots = 0;
    if (entry != 0) {
        for (uint32_t index = 0; index < functionCount_; ++index) {
            const uint8_t *record = table_ + index * BytecodeFormat::FunctionRecordLength;
            if (read16(record) == entry) {
                roots = read16(record + 10);
                break;
            }
        }
        if (roots + 2 > floor_) {
            fail(Error::Format);
            return;
        }
    }
    // 一時領域を根に含めず、回収後に古い参照も残さない。
    for (uint32_t index = globalRootCount_; index < globalCount; ++index) {
        globals[index] = Value{};
    }
    for (uint32_t index = entry == 0 ? 0 : 2 + roots; index < sp; ++index) {
        stack_[index] = Value{};
    }
    for (BytecodeHeap::Block *block = heap_.blocks; block; block = block->next) {
        block->marked = false;
    }
    markValues(globals.get(), globalRootCount_);
    if (entry != 0) {
        markValues(stack_.get() + 2, roots);
    }
    heap_.sweep();
    bool used[MaxStrings]{};
    useTextSlots(globals.get(), globalRootCount_, used);
    if (entry != 0) {
        useTextSlots(stack_.get() + 2, roots, used);
    }
    for (const BytecodeHeap::Block *block = heap_.blocks; block; block = block->next) {
        useTextSlots(static_cast<const Value *>(block->data), block->valueCount, used);
    }
    for (uint32_t index = 0; index < MaxStrings; ++index) {
        if (!used[index]) {
            texts_[index].bytes.reset();
            texts_[index].length = 0;
        }
    }
}

/**
 * @brief Value列と数値バイト列を持つ本体を確保し、GC参照を返す。
 * @param classId 非0の本体型ID。
 * @param valueCount 本体先頭のValue数。
 * @param byteCount Value列の後ろに置く生バイト数。
 * @param value 結果の値。同期C++処理が終了する前にVMへ返し、C++側へ持ち越さない。
 * @return 成功時true。失敗時は出力値を変えず実行エラーにする。
 * @details 本体はフレームに属さず、代入・引数・returnは参照を共有する。参照数は更新しない。
 * 確保中にGC・再試行は行わず、確保失敗はMemoryとする。
 * 型表とValue数を照合し、文字列フィールドは文字列プールの参照探索でも保持する。
 */
bool BytecodeVM::allocateHeapValue(uint16_t classId, uint32_t valueCount, size_t byteCount, Value &value) {
    if (state != State::Running || !stack_) {
        return false;
    }
    const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, classId);
    if (!record || record[8] != 0 || valueCount != read16(record + 2) || byteCount != 0) {
        return fail(Error::Type);
    }
    const size_t maxPayload = MaxHeapBytes - sizeof(BytecodeHeap::Block);
    if (byteCount > maxPayload || valueCount > (maxPayload - byteCount) / sizeof(Value)) {
        return fail(Error::Limit);
    }
    const size_t bytes = size_t(valueCount) * sizeof(Value) + byteCount;
    BytecodeHeap::Block *block = heap_.allocate(bytes);
    if (!block) {
        return fail(Error::Memory);
    }
    block->classId = classId;
    block->valueCount = valueCount;
    Value *values = static_cast<Value *>(block->data);
    for (uint32_t index = 0; index < valueCount; ++index) {
        new (values + index) Value{};
    }
    value = Value{};
    value.type = Type::HeapReference;
    value.classId = classId;
    value.reference = block;
    return true;
}

/**
 * @copydoc BytecodeVM::Array::getOffset
 */
bool BytecodeVM::Array::getOffset(const Value *indices, uint8_t count, uint32_t &offset) const {
    if (count != rank) {
        return false;
    }
    offset = 0;
    for (uint8_t axis = 0; axis < rank; ++axis) {
        if (indices[axis].type != Type::Integer) {
            return false;
        }
        int32_t index = indices[axis].integer;
        if (index < 0) {
            index += int32_t(shape[axis]);
        }
        if (index < 0 || uint32_t(index) >= shape[axis]) {
            return false;
        }
        offset = offset * shape[axis] + uint32_t(index);
    }
    return true;
}

/**
 * @copydoc BytecodeVM::allocateArray
 */
bool BytecodeVM::allocateArray(uint16_t classId, uint8_t dtype, const uint32_t *shape, uint8_t rank, Value &value) {
    if (state != State::Running || !stack_) {
        return false;
    }
    const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, classId);
    if (!record || dtype < Array::Float32 || dtype > Array::Object || record[8] != dtype || record[9] != rank) {
        return fail(Error::Type);
    }
    const size_t itemBytes = dtype == Array::Float32 ? sizeof(float) : dtype == Array::UInt8 ? 1 : sizeof(Value);
    const size_t maximum = (MaxHeapBytes - sizeof(BytecodeHeap::Block) - sizeof(Array)) / itemBytes;
    uint32_t elements = 0;
    if (!arrayElements(shape, rank, maximum, elements)) {
        return fail(Error::ArrayShape);
    }
    const size_t bytes = sizeof(Array) + size_t(elements) * itemBytes;
    BytecodeHeap::Block *block = heap_.allocate(bytes);
    if (!block) {
        return fail(Error::Memory);
    }
    block->classId = classId;
    block->arrayKind = dtype;
    block->valueCount = dtype == Array::Object ? elements : 0;
    auto *values = static_cast<Value *>(block->data);
    for (uint32_t index = 0; index < block->valueCount; ++index) {
        new (values + index) Value{};
        values[index].type = Type::None;
    }
    auto *array = new (values + block->valueCount) Array{};
    array->rank = rank;
    array->dtype = dtype;
    array->elements = elements;
    for (uint8_t axis = 0; axis < rank; ++axis) {
        array->shape[axis] = shape[axis];
    }
    // ヘッダー直後を型付きの数値要素として初期化し、以降は直接読み書きする。
    if (dtype == Array::Float32) {
        auto *data = reinterpret_cast<float *>(array + 1);
        for (uint32_t index = 0; index < elements; ++index) {
            new (data + index) float(0.0f);
        }
    }
    else if (dtype == Array::UInt8) {
        auto *data = reinterpret_cast<uint8_t *>(array + 1);
        for (uint32_t index = 0; index < elements; ++index) {
            new (data + index) uint8_t(0);
        }
    }
    value = Value{};
    value.type = Type::HeapReference;
    value.classId = classId;
    value.reference = block;
    return true;
}

/**
 * @copydoc BytecodeVM::arrayData
 */
bool BytecodeVM::arrayData(const Value &value, const Array *&array) const {
    if (value.type != Type::HeapReference || !value.reference) {
        return false;
    }
    const BytecodeHeap::Block &block = *value.reference;
    if (block.classId != value.classId || block.arrayKind < Array::Float32 || block.arrayKind > Array::Object) {
        return false;
    }
    // 数値配列はvalueCount=0。object配列はValue要素列の直後にヘッダーを置く。
    array = reinterpret_cast<const Array *>(static_cast<const Value *>(block.data) + block.valueCount);
    return true;
}

/**
 * @copydoc BytecodeVM::arrayElementValue
 */
bool BytecodeVM::arrayElementValue(uint16_t classId, const Value &value) const {
    if (value.type == Type::None) {
        return true;
    }
    const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, classId);
    return record && uint8_t(value.type) == record[10] && value.classId == read16(record + 12);
}

/**
 * @copydoc BytecodeVM::heapFields
 */
bool BytecodeVM::heapFields(const Value &value, uint16_t classId, uint32_t count, const Value *&fields) const {
    if (value.type != Type::HeapReference || value.classId != classId || !heap_.contains(value.reference) ||
        value.reference->classId != classId || value.reference->arrayKind != 0 ||
        value.reference->valueCount != count || value.reference->bytes != size_t(count) * sizeof(Value)) {
        return false;
    }
    const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, classId);
    if (!record || record[8] != 0 || read16(record + 2) != count) {
        return false;
    }
    fields = static_cast<const Value *>(value.reference->data);
    return true;
}

/**
 * @copydoc BytecodeVM::fieldValue
 */
bool BytecodeVM::fieldValue(const Value &value, const uint8_t *field) const {
    if (value.type == Type::None && (field[0] == uint8_t(Type::HeapReference) || field[0] == uint8_t(Type::Object))) {
        return true;
    }
    return uint8_t(value.type) == field[0] &&
        ((value.type != Type::HeapReference && value.type != Type::Object) || value.classId == read16(field + 2)) &&
        validValue(value);
}

/**
 * @brief エラー状態へ移る。
 * @param code 理由。
 * @return false。
 * @details 診断用の値と登録状態は、次のstop/startまで保持する。
 */
bool BytecodeVM::fail(Error code) {
    error = code;
    state = State::Error;
    return false;
}

/**
 * @brief 借用バッファを参照しない状態へ戻す。数値の大域変数は保持する。
 * @details
 * 旧イメージを読まず、文字列・Object・HeapReferenceの大域値を無効化し、動的文字列を解放する。
 * 数値の大域値はstop後も参照できる。
 * NativeApi.resetの後でGC本体を全解放する。C++側はGC対象の参照を保持しない。
 */
void BytecodeVM::stop() {
#ifdef __EMSCRIPTEN__
    browserCallActive_ = false;
#endif
    debugAction_ = DebugAction::Continue;
    skipDebugArrival_ = false;
    nativePending_ = false;
    if (state != State::Idle) {
        state = State::Stopped;
    }
    code_ = nullptr;
    table_ = nullptr;
    types_ = nullptr;
    typeCount_ = 0;
    strings_ = nullptr;
    stringData_ = nullptr;
    stringCount_ = 0;
    stringDataLength_ = 0;
    for (uint32_t index = 0; globals && index < globalCount; ++index) {
        if (globals[index].type == Type::String || globals[index].type == Type::Object ||
            globals[index].type == Type::HeapReference) {
            globals[index] = Value{};
        }
    }
    stack_.reset();
    nativeResults_.reset();
    nativeResultSlots_ = 0;
    sp = 0;
    bp = 0;
    depth_ = 0;
    for (Text &text : texts_) {
        text.bytes.reset();
        text.length = 0;
    }
    // 実際にスタブを呼んだ実行にはNativeApi.resetを1回呼び、ハンドルの破棄を委譲する。
    if (nativeActive_ && nativeApi_ && nativeApi_->reset) {
        nativeApi_->reset(nativeApi_->context);
    }
    nativeActive_ = false;
    heap_.clear();
}

/**
 * @brief 登録表を検査して接続する。
 * @param api 表またはnull。
 * @return 成功時true。
 * @details 停止中だけ許可する。登録表は非0 IDの昇順、入出力は展開後の幅。
 */
bool BytecodeVM::bindNativeApi(const NativeApi *api) {
    if (state == State::Running || state == State::Paused) {
        return false;
    }
    if (api) {
        if (!api->abiHash || api->count > 65535 || (api->count != 0 && !api->entries)) {
            return false;
        }
        uint16_t previous = 0;
        for (size_t index = 0; index < api->count; ++index) {
            const NativeEntry &entry = api->entries[index];
            if (entry.id <= previous || !entry.invoke || entry.argumentSlots > MaxStack ||
                entry.resultSlots == 0 || entry.resultSlots > MaxStack) {
                return false;
            }
            previous = entry.id;
        }
    }
    stop();
    nativeApi_ = api;
    return true;
}

/**
 * @brief 検証済みイメージの開始を準備する。
 * @param image 全体。
 * @param length 長さ。
 * @return 成功時true。
 *
 * @details
 * イメージ・文字列表は借用する。置換後は次のtickより前にstop/startするmain.cppの規約を守る。
 * 大域変数と追加の局所変数は未初期化で用意し、LOADでエラーにする。
 * 大域1024/スタック1024/ユーザー関数64段の上限はバイナリ形式の上限とは別の実行上限。
 */
bool BytecodeVM::start(const uint8_t *image, size_t length) {
    stop();
    state = State::Idle;
    error = Error::None;
    errorPc = 0;
    pc = 0;
    sp = 0;
    bp = 0;
    instructions = 0;
    pauseReason = 1;
    frameId_ = 1;
    nextFrameId_ = 1;
    globals.reset();
    globalCount = 0;
    floor_ = 0;
    base_ = 0;
    resultSlots_ = 1;
    BytecodeFormat::Image view;
    if (!BytecodeFormat::parseImage(image, length, view)) {
        return fail(Error::Format);
    }
    globalCount = view.globalCount;
#ifdef __EMSCRIPTEN__
    if (view.kernelHash && (!kernelApi_ || std::memcmp(view.kernelHash, kernelApi_->hash, 32) != 0)) {
        return fail(Error::AbiMismatch);
    }
#endif
    globalRootCount_ = view.globalRootCount;
    functionCount_ = view.functionCount;
    codeLength_ = view.codeLength;
    stringCount_ = view.stringCount;
    stringDataLength_ = view.stringDataLength;
    if (globalCount > MaxGlobals) {
        return fail(Error::Limit);
    }
    // 登録済みなら指紋を照合する。未登録でも純粋な計算は実行できる。
    std::memcpy(abiHash_, view.abiHash, sizeof abiHash_);
    if (nativeApi_ && std::memcmp(abiHash_, nativeApi_->abiHash, sizeof abiHash_) != 0) {
        return fail(Error::AbiMismatch);
    }
    table_ = view.functions;
    types_ = view.types;
    typeCount_ = view.typeCount;
    code_ = view.code;
    strings_ = view.strings;
    stringData_ = view.stringData;
    uint32_t stringEnd = 0;
    for (size_t index = 0; index < stringCount_; ++index) {
        const uint8_t *record = strings_ + index * 8;
        const uint32_t begin = read32(record);
        const uint32_t count = read32(record + 4);
        if (begin != stringEnd || begin > stringDataLength_ || count > stringDataLength_ - begin) {
            return fail(Error::Format);
        }
        stringEnd = begin + count;
    }
    if (stringEnd != stringDataLength_) {
        return fail(Error::Format);
    }
    end_ = functionCount_ == 0 ? codeLength_ : read16(table_);
    uint32_t previousEnd = end_;
    if (end_ == 0 || end_ > codeLength_) {
        return fail(Error::Format);
    }
    for (size_t index = 0; index < functionCount_; ++index) {
        const uint8_t *record = table_ + index * BytecodeFormat::FunctionRecordLength;
        const uint32_t begin = read16(record);
        const uint32_t end = begin + read16(record + 2);
        if (begin != previousEnd || end <= begin || end > codeLength_) {
            return fail(Error::Format);
        }
        if (read16(record + 8) == 0 || read16(record + 10) < read16(record + 4) ||
            read16(record + 10) > uint32_t(read16(record + 4)) + read16(record + 6)) {
            return fail(Error::Format);
        }
        previousEnd = end;
    }
    if (previousEnd != codeLength_) {
        return fail(Error::Format);
    }
    if (globalCount > 0) {
        globals.reset(new (std::nothrow) Value[globalCount]);
        if (!globals) {
            return fail(Error::Memory);
        }
    }
    stack_.reset(new (std::nothrow) Value[MaxStack]);
    if (!stack_) {
        return fail(Error::Memory);
    }
    if (nativeApi_) {
        for (size_t index = 0; index < nativeApi_->count; ++index) {
            if (nativeApi_->entries[index].resultSlots > nativeResultSlots_) {
                nativeResultSlots_ = nativeApi_->entries[index].resultSlots;
            }
        }
    }
    if (nativeResultSlots_ > 0) {
        nativeResults_.reset(new (std::nothrow) Value[nativeResultSlots_]);
        if (!nativeResults_) {
            return fail(Error::Memory);
        }
    }
    state = State::Running;
    return true;
}

/**
 * @brief 一時値を積む。
 * @param value 値。
 * @return 成功時true。
 */
bool BytecodeVM::push(const Value &value) {
    if (sp >= MaxStack) {
        return fail(Error::StackOverflow);
    }
    stack_[sp++] = value;
    return true;
}

/**
 * @brief 固定領域を侵さず一時値を取り出す。
 * @param value 出力。
 * @return 成功時true。
 *
 * @details
 * 関数ごとに一時スタックの下限を持ち、POPや演算で引数・局所変数・管理情報を取り出せないようにする。
 */
bool BytecodeVM::pop(Value &value) {
    if (sp <= floor_) {
        return fail(Error::StackUnderflow);
    }
    value = stack_[--sp];
    return true;
}

/**
 * @brief 数値か調べる。
 * @param value 値。
 * @return 数値ならtrue。
 */
bool BytecodeVM::numeric(const Value &value) {
    return value.type == Type::Integer || value.type == Type::Float;
}

/**
 * @brief 数値・文字列・参照・Noneの条件判定。
 * @param value 値。
 * @param result 真偽。
 * @return 有効ならtrue。
 *
 * @details
 * 数値0・None・空文字列は偽。有効な機器・通常GC本体は真。ndarrayの真偽変換はTypeで拒否する。
 * __bool__等の特殊メソッドは呼ばない。長さや要素の比較で条件を明示する。
 */
bool BytecodeVM::truth(const Value &value, bool &result) {
    if (value.type == Type::None) {
        result = false;
    }
    else if (value.type == Type::Integer) {
        result = value.integer != 0;
    }
    else if (value.type == Type::Float) {
        result = value.real != 0;
    }
    else if (value.type == Type::HeapReference || value.type == Type::Object) {
        if (!validValue(value)) {
            return fail(Error::Type);
        }
        if (value.type == Type::HeapReference && value.reference->arrayKind != 0) {
            return fail(Error::Type);
        }
        result = true;
    }
    else if (value.type == Type::String) {
        const char *data = nullptr;
        size_t length = 0;
        if (!stringBytes(value, data, length)) {
            return fail(Error::Type);
        }
        result = length != 0;
    }
    else {
        return fail(value.type == Type::Uninitialized ? Error::Uninitialized : Error::Type);
    }
    return true;
}

/**
 * @brief 四則または比較を計算する。
 * @param opcode ADD〜GEの四則・比較命令。
 *
 * @details
 * 整数同士のADD・SUB・MULはint64で中間計算し、int32範囲を超えたらエラーにする。floatを含む演算とDIVはfloat32の結果を作り、非有限値になったら停止する。DIVは整数同士でもfloatを返し、0除算はエラー。比較は整数値を先にfloat32へ丸めず、doubleへ拡張して行う。NoneのEQ／NEはバイトコード仕様に従い、Noneの大小比較・四則は型エラー。
 * 本体のEQ/NEは参照同一性、文字列のEQ/NEはUTF-8の内容で比較する。__eq__等の演算子オーバーロードは呼ばない。
 */
void BytecodeVM::calculate(uint8_t opcode) {
    Value right;
    Value left;
    if (!pop(right) || !pop(left)) {
        return;
    }
    Value result;
    if (opcode >= Opcode::EQ && (left.type == Type::None || right.type == Type::None)) {
        if (opcode != Opcode::EQ && opcode != Opcode::NE) {
            fail(Error::Type);
            return;
        }
        const bool equal = left.type == Type::None && right.type == Type::None;
        result.type = Type::Integer;
        result.integer = opcode == Opcode::EQ ? equal : !equal;
        push(result);
        return;
    }
    if ((opcode == Opcode::EQ || opcode == Opcode::NE) &&
        (left.type == Type::HeapReference || right.type == Type::HeapReference || left.type == Type::Object ||
            right.type == Type::Object)) {
        const bool equal = left.type == right.type && left.classId == right.classId &&
            (left.type == Type::HeapReference ? left.reference == right.reference : left.address == right.address);
        result.type = Type::Integer;
        result.integer = opcode == Opcode::EQ ? equal : !equal;
        push(result);
        return;
    }
    if ((opcode == Opcode::EQ || opcode == Opcode::NE) && (left.type == Type::String || right.type == Type::String)) {
        bool equal = false;
        if (left.type == Type::String && right.type == Type::String) {
            const char *a = nullptr;
            const char *b = nullptr;
            size_t aLength = 0;
            size_t bLength = 0;
            if (!stringBytes(left, a, aLength) || !stringBytes(right, b, bLength)) {
                fail(Error::Type);
                return;
            }
            equal = aLength == bLength && std::memcmp(a, b, aLength) == 0;
        }
        result.type = Type::Integer;
        result.integer = opcode == Opcode::EQ ? equal : !equal;
        push(result);
        return;
    }
    if (!numeric(left) || !numeric(right)) {
        fail(Error::Type);
        return;
    }
    const double a = left.type == Type::Integer ? double(left.integer) : double(left.real);
    const double b = right.type == Type::Integer ? double(right.integer) : double(right.real);
    if (opcode >= Opcode::EQ) {
        result.type = Type::Integer;
        switch (opcode) {
            case Opcode::EQ:
                result.integer = a == b;
                break;
            case Opcode::NE:
                result.integer = a != b;
                break;
            case Opcode::LT:
                result.integer = a < b;
                break;
            case Opcode::LE:
                result.integer = a <= b;
                break;
            case Opcode::GT:
                result.integer = a > b;
                break;
            case Opcode::GE:
                result.integer = a >= b;
                break;
        }
    }
    else if (opcode != Opcode::DIV && left.type == Type::Integer && right.type == Type::Integer) {
        int64_t integer = 0;
        if (opcode == Opcode::ADD) {
            integer = int64_t(left.integer) + right.integer;
        }
        else if (opcode == Opcode::SUB) {
            integer = int64_t(left.integer) - right.integer;
        }
        else {
            integer = int64_t(left.integer) * right.integer;
        }
        if (integer < INT32_MIN || integer > INT32_MAX) {
            fail(Error::IntegerOverflow);
            return;
        }
        result.type = Type::Integer;
        result.integer = int32_t(integer);
    }
    else {
        if (opcode == Opcode::DIV && b == 0) {
            fail(Error::DivisionByZero);
            return;
        }
        result.type = Type::Float;
        result.real = float(opcode == Opcode::ADD ? a + b
                : opcode == Opcode::SUB           ? a - b
                : opcode == Opcode::MUL           ? a * b
                                                  : a / b);
        if (!std::isfinite(result.real)) {
            fail(Error::NonFinite);
            return;
        }
    }
    push(result);
}

/**
 * @brief 引数の前に管理領域を挿入する。
 * @param entry 関数入口PC。
 *
 * @details
 * ## 関数フレーム
 *
 * CALLは関数表から引数数nと追加局所数mを得る。引数の直前に戻り先PC・旧BPの2スロットを挿入し、BP+2から仮引数、その後へ局所変数を並べる。関数呼出し以前の式の一時値はその下に残す。管理領域は専用の値種別で持ち、LOAD_L／STORE_Lから触れない。
 * 入口PCで関数表を検索し、展開後の引数・局所・戻り値幅を使う。
 */
void BytecodeVM::call(uint16_t entry) {
    size_t low = 0;
    size_t high = functionCount_;
    while (low < high) {
        const size_t middle = (low + high) / 2;
        if (read16(table_ + middle * BytecodeFormat::FunctionRecordLength) < entry) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if (low == functionCount_ || read16(table_ + low * BytecodeFormat::FunctionRecordLength) != entry) {
        fail(Error::Operand);
        return;
    }
    const uint8_t *record = table_ + low * BytecodeFormat::FunctionRecordLength;
    const uint32_t args = read16(record + 4);
    const uint32_t locals = read16(record + 6);
    if (sp < floor_ || args > sp - floor_) {
        fail(Error::StackUnderflow);
        return;
    }
    if (depth_ >= MaxFrames) {
        fail(Error::FrameOverflow);
        return;
    }
    if (sp + 2 + locals > MaxStack) {
        fail(Error::StackOverflow);
        return;
    }
    const uint32_t base = sp - args;
    std::memmove(stack_.get() + base + 2, stack_.get() + base, args * sizeof(Value));
    // BP+0は戻りPC、+1は旧BP、+2から引数と局所。
    stack_[base].type = Type::ReturnPc;
    stack_[base].address = pc;
    stack_[base + 1].type = Type::SavedBp;
    stack_[base + 1].address = bp;
    for (uint32_t index = 0; index < locals; ++index) {
        stack_[base + 2 + args + index] = Value{};
    }
    if (nextFrameId_ == UINT32_MAX) {
        fail(Error::Limit);
        return;
    }
    frames_[depth_++] = {base_, end_, floor_, resultSlots_, frameId_};
    frameId_ = ++nextFrameId_;
    bp = base;
    base_ = entry;
    end_ = entry + read16(record + 2);
    floor_ = base + 2 + args + locals;
    sp = floor_;
    pc = entry;
    resultSlots_ = read16(record + 8);
}

/**
 * @brief 戻り値を残し、引数・局所変数・管理領域を解放する。
 *
 * @details
 * RETは宣言幅の値スロットを呼出し元の引数位置へ移し、フレームを解放する。再帰と値コピーに対応する。
 * 宣言幅ちょうどの一時値を先頭からコピーする。Noneの戻り値は従来通り1スロット。
 * HeapReferenceは本体をコピー・移動せず共有する。calleeの局所参照はspの縮小で根から外れる。
 * スロットの移動途中ではGCを実行しない。Pythonの本体returnも参照1個を共有し、内部の本体を移動しない。
 * ブラウザRPCの最外フレームだけは、戻り値をスタックに残してHaltedへ移る。
 * ホストは結果をコピーしてreleaseBrowserCall()を呼ぶ。通常のCALLからの復帰は変更しない。
 */
void BytecodeVM::ret() {
    if (depth_ == 0 || bp + 1 >= floor_ || stack_[bp].type != Type::ReturnPc ||
        stack_[bp + 1].type != Type::SavedBp) {
        fail(Error::Return);
        return;
    }
    if (sp < floor_ || sp - floor_ != resultSlots_) {
        fail(Error::Return);
        return;
    }
    const Frame caller = frames_[depth_ - 1];
    const uint32_t returnPc = stack_[bp].address;
    const uint32_t oldBp = stack_[bp + 1].address;
    if (returnPc < caller.base || returnPc >= caller.end || oldBp > bp) {
        fail(Error::Return);
        return;
    }
    for (uint32_t index = 0; index < resultSlots_; ++index) {
        if (!validValue(stack_[floor_ + index])) {
            fail(Error::Type);
            return;
        }
    }
    std::memmove(stack_.get() + bp, stack_.get() + floor_, resultSlots_ * sizeof(Value));
    sp = bp + resultSlots_;
    bp = oldBp;
    base_ = caller.base;
    end_ = caller.end;
    floor_ = caller.floor;
    pc = returnPc;
    resultSlots_ = caller.results;
    frameId_ = caller.id;
    --depth_;
#ifdef __EMSCRIPTEN__
    // 外部RPCの最外RETでは呼出し元の命令を再開しない。結果はVMスタックへ残す。
    if (browserCallActive_ && depth_ == 0) {
        state = State::Halted;
    }
#endif
}

/**
 * @brief 外部へ渡せる値か検査する。
 * @param value 対象。
 * @return 有効ならtrue。
 */
bool BytecodeVM::validValue(const Value &value) const {
    if (value.type == Type::Integer || value.type == Type::None) {
        return true;
    }
    if (value.type == Type::Float) {
        return std::isfinite(value.real);
    }
    // ObjectはclassId:uint16とhandle:uint32の不透明値で、VMはネイティブポインターを保持しない。
    if (value.type == Type::Object) {
        return value.classId != 0;
    }
    if (value.type == Type::HeapReference) {
        const Value *fields = nullptr;
        const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, value.classId);
        if (record && record[8] != 0) {
            const Array *array = nullptr;
            return arrayData(value, array);
        }
        return record && heapFields(value, value.classId, read16(record + 2), fields);
    }
    const char *data = nullptr;
    size_t length = 0;
    return stringBytes(value, data, length);
}

/**
 * @copydoc BytecodeVM::callNumpy
 */
void BytecodeVM::callNumpy(uint16_t classId) {
    const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, classId);
    if (!record || record[8] == 0 || record[9] < 1 || record[9] > 4) {
        fail(Error::Type);
        return;
    }
    Value function;
    Value count;
    if (!pop(function) || !pop(count)) {
        return;
    }
    if (function.type != Type::Integer || function.integer < 1 || function.integer > 65535 ||
        count.type != Type::Integer || count.integer < 0 || count.integer > 4) {
        fail(Error::ArrayOption);
        return;
    }
    numpy_native::Option options[4];
    for (int index = count.integer - 1; index >= 0; --index) {
        Value id;
        if (!pop(options[index].value) || !pop(id)) {
            return;
        }
        if (id.type != Type::Integer || id.integer < 0 || id.integer > 65535) {
            fail(Error::ArrayOption);
            return;
        }
        options[index].id = uint16_t(id.integer);
    }
    Value shape[4];
    for (int axis = record[9] - 1; axis >= 0; --axis) {
        if (!pop(shape[axis])) {
            return;
        }
    }
    // 受入れ可能な入力は整数・文字列・Noneだけ。検証後の確保中に生きた本体参照をC++へ退避しない。
    Value result;
    const Error code = numpy_native::create(
        *this, uint16_t(function.integer), classId, shape, record[9], options, size_t(count.integer), result);
    if (code != Error::None) {
        fail(code);
    }
    else {
        push(result);
    }
}

/**
 * @copydoc BytecodeVM::arrayItem
 */
void BytecodeVM::arrayItem(uint16_t rank, bool write) {
    if (rank < 1 || rank > 4) {
        fail(Error::ArrayIndex);
        return;
    }
    const uint32_t count = rank + 1 + (write ? 1 : 0);
    if (sp < floor_ || count > sp - floor_) {
        fail(Error::StackUnderflow);
        return;
    }
    const uint32_t first = sp - count;
    const Value &receiver = stack_[first + (write ? 1 : 0)];
    const Value *indices = &receiver + 1;
    Value value = write ? stack_[first] : Value{};
    // 同期処理中に確保・GCを行わないため、対象と添字はスタック上を直接参照する。
    const Error code = numpy_native::item(*this, receiver, indices, uint8_t(rank), write, value);
    if (code != Error::None) {
        fail(code);
        return;
    }
    sp = first;
    if (!write) {
        push(value);
    }
}

/**
 * @copydoc BytecodeVM::arrayAttribute
 */
void BytecodeVM::arrayAttribute(uint16_t kind) {
    Value axis;
    Value receiver;
    Value result;
    if ((kind == 4 && !pop(axis)) || !pop(receiver)) {
        return;
    }
    const Error code = numpy_native::attribute(*this, receiver, kind, axis, result);
    if (code != Error::None) {
        fail(code);
    }
    else {
        push(result);
    }
}

/**
 * @brief 単一IDのスタブへスタック上の引数を渡す。
 * @param id 呼出しID。
 * @details
 * CALL_NATIVEは単一IDでNativeApiの表を引く。
 * メソッドはselfのObjectハンドルを先頭に積み、コンストラクターはselfを受け取らない。
 * 引数は読取専用、結果はstart()で確保した再利用領域を毎回未初期化へ戻し、成功後だけ引数と置換する。BPは変更しない。
 * 登録表の最大結果幅だけを確保し、呼出しやNativePendingの再試行ごとにはnew/deleteしない。
 * スタブは各引数型・classId・ハンドルの有効性を検査し、全結果を宣言順に設定する責任を持つ。
 * Floatの有限性は本関数がvalidValue()で呼出し前後に検査し、非有限値はTypeで停止する。
 * device_api.generated.cppの数学関数は共通変換から標準関数へ直接接続し、この検査を使う。
 * コールバックはloop()で短時間に同期実行し、VM再入は禁止。
 * 未登録・ID欠落はエラー14、ABI不一致は15。スタブの失敗は返却されたErrorで停止する。
 * 生成表はdevice_api.generated.cpp、機器操作はdevice_native.cppが担当する。
 * NativePendingは待機通知で、PCと引数を保って次のtickで同じ命令を再試行する。
 * 待機時は結果を返さず、バックエンドが処理の進捗を保持する。成功した呼出しもloop()へ戻し、
 * 1回のtickで複数のI2C操作や長い転送を連続実行しない。instructionsは再試行回数も数える。
 * sleepもこの契約を使い、時間判定はdevice_native.cppのfunction_sleepが行う。
 * 待機中のstateはRunningで、tick()はC++のloop()へ戻る。完了後は保持した次の命令へ進み、
 * sleepの引数式や呼出し元の文を最初から実行し直さない。
 * コールバック中はGCを行わず、成功時の結果をVMへコピーしてC++側の結果領域を消去する。
 * 成功・待機・失敗のいずれも、GC対象のValue・本体ポインター・内部ポインターをC++側へ持ち越さない。
 * バックエンドが待機中に保持できるのは機器状態や独立したコピーであり、GC対象への参照ではない。
 */
void BytecodeVM::callNative(uint16_t id) {
    if (!nativeApi_ || id == 0) {
        fail(Error::NativeUnavailable);
        return;
    }
    // start()に加え、呼出し直前にもABIハッシュを照合する。
    if (std::memcmp(abiHash_, nativeApi_->abiHash, sizeof abiHash_) != 0) {
        fail(Error::AbiMismatch);
        return;
    }
    size_t low = 0;
    size_t high = nativeApi_->count;
    while (low < high) {
        const size_t middle = (low + high) / 2;
        if (nativeApi_->entries[middle].id < id) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if (low == nativeApi_->count || nativeApi_->entries[low].id != id) {
        fail(Error::NativeUnavailable);
        return;
    }
    const NativeEntry &entry = nativeApi_->entries[low];
    if (sp < floor_ || entry.argumentSlots > sp - floor_) {
        fail(Error::StackUnderflow);
        return;
    }
    const uint32_t first = sp - entry.argumentSlots;
    if (entry.resultSlots > MaxStack - first) {
        fail(Error::StackOverflow);
        return;
    }
    for (uint32_t index = first; index < sp; ++index) {
        if (!validValue(stack_[index])) {
            fail(Error::Type);
            return;
        }
    }
    if (!nativeResults_ || entry.resultSlots > nativeResultSlots_) {
        fail(Error::Operand);
        return;
    }
    for (uint32_t index = 0; index < entry.resultSlots; ++index) {
        nativeResults_[index] = Value{};
    }
    NativeCall call{*this, stack_.get() + first, nativeResults_.get(), entry.argumentSlots, entry.resultSlots,
        nativeApi_->context, id};
    nativeActive_ = true;
    Error result = entry.invoke(call);
    yielded_ = true;
    if (result == Error::None) {
        for (uint32_t index = 0; index < entry.resultSlots; ++index) {
            if (!validValue(nativeResults_[index])) {
                result = Error::Type;
                break;
            }
        }
        if (result == Error::None) {
            std::memcpy(stack_.get() + first, nativeResults_.get(), entry.resultSlots * sizeof(Value));
            sp = first + entry.resultSlots;
        }
    }
    // C++の結果領域へGC参照を持ち越さず、成功時はVMの値だけが保持する。
    for (uint32_t index = 0; index < entry.resultSlots; ++index) {
        nativeResults_[index] = Value{};
    }
    if (result == Error::NativePending) {
        nativePending_ = true;
        pc = errorPc;
        return;
    }
    if (result != Error::None) {
        fail(result);
        return;
    }
}

/**
 * @brief 定数または動的Stringの長さ付き参照を得る。
 * @param value 値。
 * @param data 出力。
 * @param length バイト長。
 * @return 有効ならtrue。
 * @details
 * 埋込NULを許す。ネイティブ側で非同期に保持する場合はコピーする。
 * 定数文字列の長さはイメージ上限に従う。
 */
bool BytecodeVM::stringBytes(const Value &value, const char *&data, size_t &length) const {
    data = nullptr;
    length = 0;
    if (value.type != Type::String) {
        return false;
    }
    if ((value.address & 0x80000000U) != 0) {
        const uint32_t index = value.address & 0x7FFFFFFFU;
        if (index >= MaxStrings || !texts_[index].bytes) {
            return false;
        }
        data = texts_[index].bytes.get();
        length = texts_[index].length;
        return true;
    }
    if (!strings_ || !stringData_ || value.address >= stringCount_) {
        return false;
    }
    const uint8_t *record = strings_ + size_t(value.address) * 8;
    const uint32_t offset = read32(record);
    length = read32(record + 4);
    if (offset > stringDataLength_ || length > stringDataLength_ - offset) {
        length = 0;
        return false;
    }
    data = reinterpret_cast<const char *>(stringData_ + offset);
    return true;
}

/**
 * @brief 空いている文字列スロットへ確保する。回収はGC_LOOPとブラウザRPCの終了境界で行う。
 * @param length UTF-8バイト数。
 * @param value 成功時の文字列ID。
 * @param bytes 書込み先。
 * @return 成功時true。空きがなければStringLimit、確保失敗はMemory。
 */
bool BytecodeVM::allocateText(size_t length, Value &value, char *&bytes) {
    if (length > MaxStringLength) {
        return fail(Error::StringLimit);
    }
    for (uint32_t index = 0; index < MaxStrings; ++index) {
        Text &text = texts_[index];
        if (!text.bytes) {
            text.bytes.reset(new (std::nothrow) char[length + 1]);
            if (!text.bytes) {
                return fail(Error::Memory);
            }
            text.length = length;
            text.bytes[length] = '\0';
            value = Value{};
            value.type = Type::String;
            value.address = 0x80000000U | index;
            bytes = text.bytes.get();
            return true;
        }
    }
    return fail(Error::StringLimit);
}

/**
 * @brief 数値書式から表示用UTF-8文字列を作る。
 * @param format 定数文字列ID。
 * @details
 * FORMAT_VALUE/BUILD_STRINGは文字列を作り、通常変数・引数・戻り値・メンバーにも保持できる。
 * 書式は意味解析の数値書式（幅、精度、b/c/d/e/E/f/F/g/G/n/o/x/X/%）を扱う。
 * ロケールはC固定。floatの精度省略時は3桁とし、型も省略した場合はf形式（.3f）で表示する。
 * 例: f"{value}"は-0.2を-0.200と表示する。幅・ゼロ埋め・明示された型と精度は保持する。
 * 型だけを指定したfloatのf/e/g/%等も精度の既定値は3。整数の書式規約は変更しない。
 * 幅は1024、精度は32まで。整数形式とcへの精度指定、不正Unicodeのcはエラー18。
 */
void BytecodeVM::formatValue(uint16_t format) {
    Value spec;
    spec.type = Type::String;
    spec.address = format;
    const char *text = nullptr;
    size_t size = 0;
    if (!stringBytes(spec, text, size)) {
        fail(Error::Operand);
        return;
    }
    size_t position = 0;
    uint32_t width = 0;
    const bool zero = size != 0 && text[0] == '0';
    while (position < size && text[position] >= '0' && text[position] <= '9') {
        width = width * 10 + uint32_t(text[position++] - '0');
        if (width > MaxStringLength) {
            fail(Error::StringLimit);
            return;
        }
    }
    int precision = -1;
    if (position < size && text[position] == '.') {
        ++position;
        precision = 0;
        const size_t begin = position;
        while (position < size && text[position] >= '0' && text[position] <= '9') {
            precision = precision * 10 + int(text[position++] - '0');
            if (precision > 32) {
                fail(Error::StringLimit);
                return;
            }
        }
        if (begin == position) {
            fail(Error::StringFormat);
            return;
        }
    }
    if (position < size && text[position] == '\0') {
        fail(Error::StringFormat);
        return;
    }
    const char type = position < size ? text[position++] : '\0';
    if (position != size || (type != '\0' && std::strchr("bcdeEfFgGnoxX%", type) == nullptr)) {
        fail(Error::StringFormat);
        return;
    }
    Value input;
    if (!pop(input)) {
        return;
    }
    if (!numeric(input)) {
        fail(Error::Type);
        return;
    }
    char buffer[MaxStringLength + 1];
    size_t length = 0;
    size_t characterCount = 0;
    const bool integer = type == 'b' || type == 'c' || type == 'd' || type == 'o' || type == 'x' || type == 'X' ||
        (input.type == Type::Integer && (type == '\0' || type == 'n'));
    if (integer) {
        if (input.type != Type::Integer || precision >= 0) {
            fail(Error::StringFormat);
            return;
        }
        if (type == 'c') {
            const uint32_t code = uint32_t(input.integer);
            if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
                fail(Error::StringFormat);
                return;
            }
            if (code < 0x80) {
                buffer[length++] = char(code);
            }
            else if (code < 0x800) {
                buffer[length++] = char(0xC0 | (code >> 6));
                buffer[length++] = char(0x80 | (code & 63));
            }
            else if (code < 0x10000) {
                buffer[length++] = char(0xE0 | (code >> 12));
                buffer[length++] = char(0x80 | ((code >> 6) & 63));
                buffer[length++] = char(0x80 | (code & 63));
            }
            else {
                buffer[length++] = char(0xF0 | (code >> 18));
                buffer[length++] = char(0x80 | ((code >> 12) & 63));
                buffer[length++] = char(0x80 | ((code >> 6) & 63));
                buffer[length++] = char(0x80 | (code & 63));
            }
            characterCount = 1;
        }
        else {
            const uint32_t radix = type == 'b' ? 2 : type == 'o' ? 8 : type == 'x' || type == 'X' ? 16 : 10;
            const char *digits = type == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            uint32_t magnitude = input.integer < 0 ? uint32_t(-int64_t(input.integer)) : uint32_t(input.integer);
            char reversed[32];
            size_t count = 0;
            do {
                reversed[count++] = digits[magnitude % radix];
                magnitude /= radix;
            } while (magnitude != 0);
            if (input.integer < 0) {
                buffer[length++] = '-';
            }
            while (count != 0) {
                buffer[length++] = reversed[--count];
            }
            characterCount = length;
        }
    }
    else {
        const double number = input.type == Type::Integer ? double(input.integer) : double(input.real);
        if (!std::isfinite(number)) {
            fail(Error::NonFinite);
            return;
        }
        char conversion = type == '%' ? 'f' : type == '\0' || type == 'n' ? 'g' : type;
        int digits = precision >= 0 ? precision : 6;
        if (input.type == Type::Float && precision < 0) {
            digits = 3;
            if (type == '\0') {
                conversion = 'f';
            }
        }
        const char pattern[] = {'%', '.', '*', conversion, '\0'};
        const int count = std::snprintf(buffer, sizeof buffer, pattern, digits, type == '%' ? number * 100 : number);
        if (count < 0 || size_t(count) > MaxStringLength - (type == '%' ? 1 : 0)) {
            fail(Error::StringLimit);
            return;
        }
        length = size_t(count);
        if (type == '%') {
            buffer[length++] = '%';
        }
        characterCount = length;
    }
    const size_t padding = width > characterCount ? width - characterCount : 0;
    Value result;
    char *bytes = nullptr;
    if (!allocateText(length + padding, result, bytes)) {
        return;
    }
    const bool negative = length != 0 && buffer[0] == '-' && type != 'c';
    if (zero && type != 'c' && negative) {
        bytes[0] = '-';
        std::memset(bytes + 1, '0', padding);
        std::memcpy(bytes + padding + 1, buffer + 1, length - 1);
    }
    else {
        std::memset(bytes, zero && type != 'c' ? '0' : ' ', padding);
        std::memcpy(bytes + padding, buffer, length);
    }
    push(result);
}

/**
 * @brief 入力を生存させたまま連結し、その後1値へ置換する。
 * @param count 断片数。
 */
void BytecodeVM::buildString(uint16_t count) {
    if (sp < floor_ || count > sp - floor_) {
        fail(Error::StackUnderflow);
        return;
    }
    const uint32_t begin = sp - count;
    size_t total = 0;
    for (uint32_t index = begin; index < sp; ++index) {
        const char *data = nullptr;
        size_t length = 0;
        if (!stringBytes(stack_[index], data, length)) {
            fail(Error::Type);
            return;
        }
        if (length > MaxStringLength - total) {
            fail(Error::StringLimit);
            return;
        }
        total += length;
    }
    Value result;
    char *bytes = nullptr;
    if (!allocateText(total, result, bytes)) {
        return;
    }
    size_t offset = 0;
    for (uint32_t index = begin; index < sp; ++index) {
        const char *data = nullptr;
        size_t length = 0;
        if (!stringBytes(stack_[index], data, length)) {
            fail(Error::Type);
            return;
        }
        std::memcpy(bytes + offset, data, length);
        offset += length;
    }
    sp = begin;
    push(result);
}

/**
 * @brief 現在の範囲内で1命令を実行する。
 */
void BytecodeVM::step() {
    errorPc = pc;
    if (pc < base_ || pc >= end_ || end_ > codeLength_) {
        fail(Error::Operand);
        return;
    }
    // bit7はbytecode_debugger.cppだけが検証済み命令先頭へ設定する。オペランドには適用しない。
    // PC・命令数・スタックを更新する前に停止し、通常Stopとは別に値と所有機器を保持する。
    const uint8_t instruction = code_[pc];
    const uint8_t opcode = instruction & BytecodeFormat::OpcodeMask;
    const uint32_t size = BytecodeFormat::instructionLength(opcode);
    if (size == 0 || size > end_ - pc) {
        fail(Error::Format);
        return;
    }
    // 再開直後の到達とNativePendingの再試行だけを除外する。ループの次周は同じPCでも再判定する。
    const bool arrival = !skipDebugArrival_ && !nativePending_;
    const bool breakpoint = arrival && (instruction & BytecodeFormat::BreakpointFlag) != 0;
    const bool point = debugPoints_ && pc < debugCodeLength_ && (debugPoints_[pc / 8] & (1U << (pc % 8))) != 0;
    const bool returned = depth_ < debugDepth_;
    const bool stepped = arrival && opcode != Opcode::HALT &&
        ((debugAction_ == DebugAction::In && (point || returned)) ||
            (debugAction_ == DebugAction::Over && ((frameId_ == debugFrameId_ && point) || returned)) ||
            (debugAction_ == DebugAction::Out && returned));
    if (breakpoint || stepped) {
        pauseReason = breakpoint ? 1 : 2;
        debugAction_ = DebugAction::Continue;
        state = State::Paused;
        return;
    }
    skipDebugArrival_ = false;
    nativePending_ = false;
    const uint16_t operand = size == 3 ? read16(code_ + pc + 1) : 0;
    const uint32_t bits = size == 5 ? read32(code_ + pc + 1) : 0;
    pc += size;
    ++instructions;
    Value value;
    switch (opcode) {
        case Opcode::LOAD_L:
        case Opcode::LOAD_G:
        case Opcode::STORE_L:
        case Opcode::STORE_G: {
            // record・ユーザー本体も1スロットの参照として読み書きする。本体のコピーは行わない。
            const bool local = opcode == Opcode::LOAD_L || opcode == Opcode::STORE_L;
            if ((local && (depth_ == 0 || operand < 2 || bp + operand >= floor_)) ||
                (!local && operand >= globalCount)) {
                fail(Error::Operand);
                return;
            }
            Value &slot = local ? stack_[bp + operand] : globals[operand];
            if (opcode == Opcode::LOAD_L || opcode == Opcode::LOAD_G) {
                if (slot.type == Type::Uninitialized) {
                    fail(Error::Uninitialized);
                    return;
                }
                push(slot);
            }
            else if (pop(value)) {
                slot = value;
            }
            break;
        }
        case Opcode::PUSH_INT:
            value.type = Type::Integer;
            std::memcpy(&value.integer, &bits, sizeof bits);
            push(value);
            break;
        case Opcode::PUSH_FLOAT:
            value.type = Type::Float;
            std::memcpy(&value.real, &bits, sizeof bits);
            if (!std::isfinite(value.real)) {
                fail(Error::NonFinite);
                return;
            }
            push(value);
            break;
        case Opcode::JUMP:
        case Opcode::JUMP_IF_FALSE:
        case Opcode::JUMP_IF_TRUE: {
            bool condition = true;
            if (opcode != Opcode::JUMP && (!pop(value) || !truth(value, condition))) {
                return;
            }
            if (opcode == Opcode::JUMP || (opcode == Opcode::JUMP_IF_FALSE && !condition) ||
                (opcode == Opcode::JUMP_IF_TRUE && condition)) {
                if (base_ + operand >= end_) {
                    fail(Error::Operand);
                    return;
                }
                pc = base_ + operand;
            }
            break;
        }
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::EQ:
        case Opcode::NE:
        case Opcode::LT:
        case Opcode::LE:
        case Opcode::GT:
        case Opcode::GE:
            calculate(opcode);
            break;
        case Opcode::CALL:
            call(operand);
            break;
        case Opcode::RET:
            ret();
            break;
        case Opcode::PUSH_NONE:
            value.type = Type::None;
            push(value);
            break;
        case Opcode::POP:
            pop(value);
            break;
        case Opcode::HALT:
            if (depth_ != 0 || sp != 0) {
                fail(Error::Operand);
                return;
            }
            // 診断用の値と登録状態は、次のstop/startまで保持する。
            state = State::Halted;
            break;
        case Opcode::PUSH_STRING: {
            // PUSH_STRINGは検証済みUTF-8定数のIDを積む。
            value.type = Type::String;
            value.address = operand;
            const char *bytes = nullptr;
            size_t length = 0;
            if (!stringBytes(value, bytes, length)) {
                fail(Error::Operand);
                return;
            }
            push(value);
            break;
        }
        case Opcode::CALL_NATIVE:
            callNative(operand);
            break;
        case Opcode::TO_FLOAT:
            // TO_FLOATはint32をfloat32に変換し、floatはそのままにする。
            if (!pop(value)) {
                return;
            }
            if (!numeric(value)) {
                fail(Error::Type);
                return;
            }
            if (value.type == Type::Integer) {
                const float converted = float(value.integer);
                value.type = Type::Float;
                value.real = converted;
            }
            push(value);
            break;
        case Opcode::FORMAT_VALUE:
            formatValue(operand);
            break;
        case Opcode::BUILD_STRING:
            buildString(operand);
            break;
        case Opcode::NEW_OBJECT: {
            const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, operand);
            if (!record) {
                fail(Error::Operand);
                return;
            }
            if (allocateHeapValue(operand, read16(record + 2), 0, value)) {
                push(value);
            }
            break;
        }
        case Opcode::GET_FIELD:
        case Opcode::SET_FIELD: {
            Value receiver;
            if (!pop(receiver)) {
                return;
            }
            if (receiver.type != Type::HeapReference || !validValue(receiver)) {
                fail(receiver.type == Type::None ? Error::NoneAccess : Error::Type);
                return;
            }
            const uint8_t *record = BytecodeFormat::findType(types_, typeCount_, receiver.classId);
            if (operand >= read16(record + 2)) {
                fail(Error::Operand);
                return;
            }
            const uint8_t *field = types_ + read32(record + 4) + size_t(operand) * 4;
            Value &slot = static_cast<Value *>(receiver.reference->data)[operand];
            const uint8_t permission = opcode == Opcode::GET_FIELD ? 1 : 2;
            if ((field[1] & permission) == 0) {
                fail(Error::Operand);
                return;
            }
            if (opcode == Opcode::GET_FIELD) {
                if (slot.type == Type::Uninitialized) {
                    fail(Error::Uninitialized);
                    return;
                }
                if (!fieldValue(slot, field)) {
                    fail(Error::Type);
                    return;
                }
                push(slot);
            }
            else {
                if (!pop(value)) {
                    return;
                }
                if (!fieldValue(value, field)) {
                    fail(Error::Type);
                    return;
                }
                slot = value;
            }
            break;
        }
        case Opcode::DUP:
            if (sp <= floor_) {
                fail(Error::StackUnderflow);
                return;
            }
            value = stack_[sp - 1];
            if (!validValue(value)) {
                fail(Error::Type);
                return;
            }
            push(value);
            break;
        case Opcode::CALL_NUMPY:
            callNumpy(operand);
            break;
        case Opcode::GET_ITEM:
        case Opcode::SET_ITEM:
            arrayItem(operand, opcode == Opcode::SET_ITEM);
            break;
        case Opcode::ARRAY_ATTR:
            arrayAttribute(operand);
            break;
        case Opcode::CHECK_REF:
            if (sp <= floor_) {
                fail(Error::StackUnderflow);
                return;
            }
            value = stack_[sp - 1];
            if (value.type == Type::None) {
                fail(Error::NoneAccess);
                return;
            }
            if ((value.type != Type::HeapReference && value.type != Type::Object) || value.classId != operand ||
                !validValue(value)) {
                fail(Error::Type);
            }
            break;
        case Opcode::GC_LOOP:
            collectHeap(operand);
            break;
#ifdef __EMSCRIPTEN__
        case Opcode::CALL_KERNEL:
            callKernel(operand);
            break;
#endif
    }
}

/**
 * @brief 停止・通信処理へ制御を返すため実行量を制限する。
 * @param budget 最大命令数。
 * @details GCはGC_LOOP命令の安全条件が成立した場合だけ行う。予算切れやC++への復帰では行わない。
 * budgetは命令数で、GC時間を含む経過時間の上限を保証しない。
 */
void BytecodeVM::tick(uint32_t budget) {
    yielded_ = false;
    for (uint32_t count = 0; count < budget && state == State::Running && !yielded_; ++count) {
        step();
    }
}

#ifdef __EMSCRIPTEN__
/**
 * @copydoc BytecodeVM::beginBrowserCall
 */
bool BytecodeVM::beginBrowserCall(
    uint16_t entry, uint16_t slot, uint16_t classId, const Value *arguments, uint16_t count) {
    if (state != State::Halted || browserCallActive_ || !code_ || !stack_ || slot >= globalRootCount_ ||
        uint32_t(count) + 1 > MaxStack) {
        return false;
    }
    const Value &self = globals[slot];
    if (self.type != Type::HeapReference || self.classId != classId || !validValue(self)) {
        return false;
    }
    stack_[0] = self;
    for (uint16_t index = 0; index < count; ++index) {
        stack_[index + 1] = arguments[index];
    }
    sp = uint32_t(count) + 1;
    bp = 0;
    floor_ = 0;
    pc = 0;
    browserCallActive_ = true;
    state = State::Running;
    call(entry);
    return state == State::Running;
}

/**
 * @copydoc BytecodeVM::copyBrowserText
 */
bool BytecodeVM::copyBrowserText(const char *data, size_t length, Value &value) {
    if (state != State::Halted || browserCallActive_) {
        return false;
    }
    char *bytes = nullptr;
    if (!allocateText(length, value, bytes)) {
        return false;
    }
    if (length != 0) {
        std::memcpy(bytes, data, length);
    }
    return true;
}

/**
 * @copydoc BytecodeVM::browserCallResult
 */
const BytecodeVM::Value *BytecodeVM::browserCallResult() const {
    return state == State::Halted && browserCallActive_ && sp == 1 ? stack_.get() : nullptr;
}

/**
 * @copydoc BytecodeVM::releaseBrowserCall
 */
bool BytecodeVM::releaseBrowserCall() {
    if (state != State::Halted || !code_ || depth_ != 0) {
        return false;
    }
    browserCallActive_ = false;
    for (uint32_t index = 0; index < sp; ++index) {
        stack_[index] = Value{};
    }
    sp = 0;
    bp = 0;
    floor_ = 0;
    state = State::Running;
    collectHeap(0);
    if (state == State::Running) {
        state = State::Halted;
    }
    return state == State::Halted;
}

/**
 * @copydoc BytecodeVM::bindKernelApi
 */
bool BytecodeVM::bindKernelApi(const KernelApi *api) {
    if (state == State::Running || state == State::Paused ||
        (api && (!api->hash || !api->argumentCount || !api->invoke))) {
        return false;
    }
    kernelApi_ = api;
    return true;
}

/**
 * @copydoc BytecodeVM::callKernel
 * @details CALL_NATIVEと別経路で、数値の有限性を検査せず、正常復帰後にホストへ制御を戻す。
 * 入出力配列の形式・共有はbrowser/host.cppが検査する。計算中はGCとVM再入を行わない。
 */
void BytecodeVM::callKernel(uint16_t id) {
    if (!kernelApi_) {
        fail(Error::NativeUnavailable);
        return;
    }
    const uint16_t count = kernelApi_->argumentCount(id);
    if (count == UINT16_MAX) {
        fail(Error::NativeUnavailable);
        return;
    }
    if (sp < floor_ || uint32_t(count) + 1 > sp - floor_) {
        fail(Error::StackUnderflow);
        return;
    }
    const uint32_t begin = sp - count - 1;
    const Value &n = stack_[begin];
    if (n.type != Type::Integer || n.integer < 0) {
        fail(Error::Operand);
        return;
    }
    const Error result = kernelApi_->invoke(*this, id, stack_.get() + begin, count + 1);
    if (result != Error::None) {
        fail(result);
        return;
    }
    sp = begin;
    Value value;
    value.type = Type::None;
    push(value);
    yielded_ = true;
}
#endif

/**
 * @brief 停止点を命令境界と照合し、1PC=1bitの表を作る。
 * @param points 昇順PC。
 * @param count 個数。
 * @param code 命令列。
 * @param length 命令列長。
 * @return 全検証と確保に成功したらtrue。
 * @details バイナリを変更せず、通常の行ブレークポイントとは別に保持する。0件は設定解除。
 */
bool BytecodeVM::configureDebugPoints(const uint16_t *points, uint32_t count, const uint8_t *code, uint32_t length) {
    if (state == State::Running || !code || length == 0 || length > 65536 || count > length || (count && !points)) {
        return false;
    }
    std::unique_ptr<uint8_t[]> bits(new (std::nothrow) uint8_t[(length + 7) / 8]{});
    if (!bits) {
        return false;
    }
    uint32_t selected = 0;
    for (uint32_t at = 0; at < length;) {
        const uint8_t opcode = code[at] & BytecodeFormat::OpcodeMask;
        const uint32_t size = BytecodeFormat::instructionLength(opcode);
        if (size == 0 || size > length - at || (selected < count && points[selected] < at)) {
            return false;
        }
        if (selected < count && points[selected] == at) {
            bits[at / 8] |= uint8_t(1U << (at % 8));
            ++selected;
        }
        at += size;
    }
    if (selected != count) {
        return false;
    }
    debugPoints_ = std::move(bits);
    debugCodeLength_ = length;
    return true;
}

/**
 * @brief 接続・プログラム世代変更でデバッグ設定を失効させる。VMの値は変更しない。
 */
void BytecodeVM::clearDebugPoints() {
    debugPoints_.reset();
    debugCodeLength_ = 0;
    debugAction_ = DebugAction::Continue;
    skipDebugArrival_ = false;
}

/**
 * @brief 現在のフレームを対象に再開・ステップを開始する。
 * @param action 実行方法。
 * @return Pausedかつ有効な操作ならtrue。
 * @details 閲覧中のフレーム選択は影響しない。Outはモジュールでは拒否する。
 * Over/Outは現在フレームの識別子と深さを保持し、復帰直後はCALLの次の命令の実行前に止める。
 * In/Overは文開始・条件評価・関数入口・暗黙returnで停止する。HALTは通常終了させる。
 */
bool BytecodeVM::resumeDebug(DebugAction action) {
    if (state != State::Paused || uint8_t(action) > uint8_t(DebugAction::Out) ||
        (action != DebugAction::Continue && (!debugPoints_ || debugCodeLength_ != codeLength_)) ||
        (action == DebugAction::Out && depth_ == 0)) {
        return false;
    }
    debugAction_ = action;
    debugFrameId_ = frameId_;
    debugDepth_ = depth_;
    skipDebugArrival_ = true;
    if (nativeApi_) {
        for (size_t index = 0; index < nativeApi_->count; ++index) {
            if (nativeApi_->entries[index].resultSlots > nativeResultSlots_) {
                nativeResultSlots_ = nativeApi_->entries[index].resultSlots;
            }
        }
    }
    if (nativeResultSlots_ > 0) {
        nativeResults_.reset(new (std::nothrow) Value[nativeResultSlots_]);
        if (!nativeResults_) {
            return fail(Error::Memory);
        }
    }
    state = State::Running;
    return true;
}

/**
 * @brief 文字列IDを初出の1回だけ送るため使用スロットを比較する。
 * @param index 大域に続けてスタックを並べた添字。
 * @return 初出Stringならtrue。
 * @details loop()だけが呼ぶ。補助配列やC++ポインターの公開は行わない。
 */
bool BytecodeVM::firstDebugString(uint32_t index) const {
    const Value &value = *debugValueAt(index);
    if (value.type != Type::String) {
        return false;
    }
    for (uint32_t previous = 0; previous < index; ++previous) {
        const Value &other = *debugValueAt(previous);
        if (other.type == Type::String && other.address == value.address) {
            return false;
        }
    }
    return true;
}

/**
 * @copydoc BytecodeVM::debugValueCount
 */
uint32_t BytecodeVM::debugValueCount() const {
    return globalCount + sp;
}

/**
 * @copydoc BytecodeVM::debugValueAt
 */
const BytecodeVM::Value *BytecodeVM::debugValueAt(uint32_t index) const {
    return index < globalCount     ? globals.get() + index
        : index - globalCount < sp ? stack_.get() + index - globalCount
                                   : nullptr;
}

/**
 * @brief 参照中の文字列を重複除去してSnapshotの必要長を計算する。
 * @return Pausedでなければ0。上限256KiB、不正な文字列でも0。
 * @details 定数文字列・動的文字列を切り詰めず送る。確保失敗はDebuggerがMemoryとして通知する。
 * HeapReferenceはclassIdだけを送る。本体ID・メンバー・配列内容をSnapshotへ含めない。
 */
size_t BytecodeVM::debugSnapshotLength() const {
    if (state != State::Paused || sp > MaxStack || globalCount > MaxGlobals ||
        (sp && !stack_) || (globalCount && !globals)) {
        return 0;
    }
    size_t length = 28 + 8 * (globalCount + sp);
    const uint32_t count = debugValueCount();
    for (uint32_t index = 0; index < count; ++index) {
        const Value &current = *debugValueAt(index);
        if (uint8_t(current.type) > uint8_t(Type::HeapReference) ||
            (current.type == Type::HeapReference && !validValue(current)) ||
            (current.type == Type::Float && !std::isfinite(current.real))) {
            return 0;
        }
        if (!firstDebugString(index)) {
            continue;
        }
        const Value &value = current;
        const char *text = nullptr;
        size_t bytes = 0;
        if (!stringBytes(value, text, bytes) || bytes > 256 * 1024 || length + 8 + bytes > 256 * 1024) {
            return 0;
        }
        length += 8 + bytes;
    }
    return length;
}

/**
 * @brief Pausedの全使用スロットと文字列をパディングなしで直列化する。
 * @param data 出力領域。
 * @param length 正確な必要長。
 * @return Pausedで正常にコピーしたらtrue。
 * @details 形式の正本はuroa/diagram/ts/ble-wifi/debug-protocol.tsのSnapshot version=6。
 * BPチェーンはReturnPc/SavedBpとdiagramのCALL表から復元する。値・文字列は同じ停止中のもの。
 */
bool BytecodeVM::copyDebugSnapshot(uint8_t *data, size_t length) const {
    if (!data || length == 0 || length != debugSnapshotLength()) {
        return false;
    }
    std::memset(data, 0, length);
    const uroa::wire::SnapshotHeader header{6, pauseReason, 8, pc, bp, sp, globalCount, sp};
    uroa::wire::writeSnapshotHeader(data, header);
    for (uint32_t index = 0; index < globalCount + sp; ++index) {
        const Value &value = index < globalCount ? globals[index] : stack_[index - globalCount];
        snapshotValue(data + 24 + index * 8, value);
    }
    size_t offset = 24 + 8 * (globalCount + sp);
    uint32_t count = 0;
    const size_t countOffset = offset;
    offset += 4;
    for (uint32_t index = 0; index < debugValueCount(); ++index) {
        if (!firstDebugString(index)) {
            continue;
        }
        const Value &value = *debugValueAt(index);
        const char *text = nullptr;
        size_t bytes = 0;
        if (!stringBytes(value, text, bytes) || offset + 8 + bytes > length) {
            return false;
        }
        const uroa::wire::StringHeader entry{value.address, static_cast<uint32_t>(bytes)};
        uroa::wire::writeStringHeader(data + offset, entry);
        std::memcpy(data + offset + 8, text, bytes);
        offset += 8 + bytes;
        ++count;
    }
    uroa::wire::writeWord32(data + countOffset, uroa::wire::Word32{count});
    return offset == length;
}
