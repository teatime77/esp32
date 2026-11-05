/** @file bytecode_vm.cpp
 * @brief 値コピーの構造体・関数フレーム・ネイティブ呼出しを持つPythonサブセットVM。
 *
 * @details
 * # PythonサブセットVM
 *
 * BytecodeReceiverで構造検証済みのversion=1/2/3を実行する。バイナリ規約は
 * uroa/diagram/ts/python/bytecode.ts先頭を参照。bytecode_vm.hは公開状態と登録API、
 * 本ファイルは実行、main.cppはloop()単位の開始・停止・実行量制御を担当する。
 * テスト・Upload・実機動作はビルドとは別の確認事項とする。
 */
#include "bytecode_vm.h"
#include <cstring>
#include <cmath>
#include <new>
#include <limits.h>
#include <cstdio>

namespace {
    /** @brief 非整列のlittle endian値を読む。@param p 2バイト。@return 数値。 */
    uint16_t read16(const uint8_t *p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }
    /** @brief 非整列のlittle endian値を読む。@param p 4バイト。@return 数値。 */
    uint32_t read32(const uint8_t *p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }
}

/** @brief エラー状態へ移る。@param code 理由。@return false。
 * @details 診断用の値と登録状態は、次のstop/startまで保持する。
 */
bool BytecodeVM::fail(Error code) {
    error = code;
    state = State::Error;
    return false;
}

/** @brief 借用バッファを参照しない状態へ戻す。数値の大域変数は保持する。
 * @details
 * 旧イメージを読まず、文字列・Objectの大域値を無効化し、動的文字列を解放する。
 * 数値の大域値はstop後も参照できる。
 */
void BytecodeVM::stop() {
    if (state != State::Idle) {
        state = State::Stopped;
    }
    code_ = nullptr;
    table_ = nullptr;
    strings_ = nullptr;
    stringData_ = nullptr;
    stringCount_ = 0;
    stringDataLength_ = 0;
    for (uint32_t index = 0; globals && index < globalCount; ++index) {
        if (globals[index].type == Type::String || globals[index].type == Type::Object) {
            globals[index] = Value{};
        }
    }
    stack_.reset();
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
}

/** @brief 登録表を検査して接続する。@param api 表またはnull。@return 成功時true。
 * @details 停止中だけ許可する。登録表は非0 IDの昇順、入出力は展開後の幅。
 */
bool BytecodeVM::bindNativeApi(const NativeApi *api) {
    if (state == State::Running) {
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

/** @brief 検証済みイメージの開始を準備する。@param image 全体。@param length 長さ。@return 成功時true。
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
    globals.reset();
    globalCount = 0;
    floor_ = 0;
    base_ = 0;
    resultSlots_ = 1;
    if (image == nullptr || length < 16 || read16(image) != 0xB1C0 || read16(image + 12) != 0) {
        return fail(Error::Format);
    }
    version_ = read16(image + 2);
    if (version_ < 1 || version_ > 3) {
        return fail(Error::Format);
    }
    // version=3の呼出し規約/API形式は2、関数表は12バイト、それ以前は8バイト。
    const size_t headerLength = version_ == 3 ? 56 : version_ == 2 ? 24 : 16;
    recordLength_ = version_ == 3 ? 12 : 8;
    if (length < headerLength || read16(image + 14) != (version_ == 3 ? 2 : 0) ||
        (version_ >= 2 && read16(image + 18) != (version_ == 3 ? 2 : 0))) {
        return fail(Error::Format);
    }
    globalCount = read16(image + 4);
    functionCount_ = read16(image + 6);
    codeLength_ = read32(image + 8);
    stringCount_ = version_ >= 2 ? read16(image + 16) : 0;
    stringDataLength_ = version_ >= 2 ? read32(image + 20) : 0;
    const size_t offset = headerLength + size_t(functionCount_) * recordLength_;
    const uint64_t expectedLength = uint64_t(offset) + codeLength_ + uint64_t(stringCount_) * 8 + stringDataLength_;
    if (length > 128 * 1024 || codeLength_ == 0 || codeLength_ > 65536 || expectedLength != length) {
        return fail(Error::Format);
    }
    if (globalCount > MaxGlobals) {
        return fail(Error::Limit);
    }
    // 登録済みならABIハッシュ32バイトを照合する。未登録でも純粋な計算は実行できる。
    if (version_ == 3) {
        std::memcpy(abiHash_, image + 24, sizeof abiHash_);
        if (nativeApi_ && std::memcmp(abiHash_, nativeApi_->abiHash, sizeof abiHash_) != 0) {
            return fail(Error::AbiMismatch);
        }
    }
    table_ = image + headerLength;
    code_ = image + offset;
    strings_ = code_ + codeLength_;
    stringData_ = strings_ + size_t(stringCount_) * 8;
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
        const uint8_t *record = table_ + index * recordLength_;
        const uint32_t begin = read16(record);
        const uint32_t end = begin + read16(record + 2);
        if (begin != previousEnd || end <= begin || end > codeLength_) {
            return fail(Error::Format);
        }
        if (version_ == 3 && (read16(record + 8) == 0 || read16(record + 10) != 0)) {
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
    state = State::Running;
    return true;
}

/** @brief 一時値を積む。@param value 値。@return 成功時true。 */
bool BytecodeVM::push(const Value &value) {
    if (sp >= MaxStack) {
        return fail(Error::StackOverflow);
    }
    stack_[sp++] = value;
    return true;
}

/** @brief 固定領域を侵さず一時値を取り出す。@param value 出力。@return 成功時true。
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

/** @brief 数値か調べる。@param value 値。@return 数値ならtrue。 */
bool BytecodeVM::numeric(const Value &value) {
    return value.type == Type::Integer || value.type == Type::Float;
}

/** @brief 数値とNoneの条件判定。@param value 値。@param result 真偽。@return 有効ならtrue。
 *
 * @details
 * 数値0とNoneは偽、それ以外の数値は真。
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
    else {
        return fail(value.type == Type::Uninitialized ? Error::Uninitialized : Error::Type);
    }
    return true;
}

/** @brief 四則または比較を計算する。@param opcode 0x0A〜0x13。
 *
 * @details
 * 整数同士のADD・SUB・MULはint64で中間計算し、int32範囲を超えたらエラーにする。floatを含む演算とDIVはfloat32の結果を作り、非有限値になったら停止する。DIVは整数同士でもfloatを返し、0除算はエラー。比較は整数値を先にfloat32へ丸めず、doubleへ拡張して行う。NoneのEQ／NEはバイトコード仕様に従い、Noneの大小比較・四則は型エラー。
 */
void BytecodeVM::calculate(uint8_t opcode) {
    Value right;
    Value left;
    if (!pop(right) || !pop(left)) {
        return;
    }
    Value result;
    if (opcode >= 0x0E && (left.type == Type::None || right.type == Type::None)) {
        if (opcode != 0x0E && opcode != 0x0F) {
            fail(Error::Type);
            return;
        }
        const bool equal = left.type == Type::None && right.type == Type::None;
        result.type = Type::Integer;
        result.integer = opcode == 0x0E ? equal : !equal;
        push(result);
        return;
    }
    if (!numeric(left) || !numeric(right)) {
        fail(Error::Type);
        return;
    }
    const double a = left.type == Type::Integer ? double(left.integer) : double(left.real);
    const double b = right.type == Type::Integer ? double(right.integer) : double(right.real);
    if (opcode >= 0x0E) {
        result.type = Type::Integer;
        switch (opcode) {
            case 0x0E:
                result.integer = a == b;
                break;
            case 0x0F:
                result.integer = a != b;
                break;
            case 0x10:
                result.integer = a < b;
                break;
            case 0x11:
                result.integer = a <= b;
                break;
            case 0x12:
                result.integer = a > b;
                break;
            case 0x13:
                result.integer = a >= b;
                break;
        }
    }
    else if (opcode != 0x0D && left.type == Type::Integer && right.type == Type::Integer) {
        int64_t integer = 0;
        if (opcode == 0x0A) {
            integer = int64_t(left.integer) + right.integer;
        }
        else if (opcode == 0x0B) {
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
        if (opcode == 0x0D && b == 0) {
            fail(Error::DivisionByZero);
            return;
        }
        result.type = Type::Float;
        result.real = float(opcode == 0x0A ? a + b : opcode == 0x0B ? a - b
            : opcode == 0x0C ? a * b : a / b);
        if (!std::isfinite(result.real)) {
            fail(Error::NonFinite);
            return;
        }
    }
    push(result);
}

/** @brief 引数の前に管理領域を挿入する。@param entry 関数入口PC。
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
        if (read16(table_ + middle * recordLength_) < entry) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if (low == functionCount_ || read16(table_ + low * recordLength_) != entry) {
        fail(Error::Operand);
        return;
    }
    const uint8_t *record = table_ + low * recordLength_;
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
    frames_[depth_++] = {base_, end_, floor_, resultSlots_};
    bp = base;
    base_ = entry;
    end_ = entry + read16(record + 2);
    floor_ = base + 2 + args + locals;
    sp = floor_;
    pc = entry;
    resultSlots_ = version_ == 3 ? read16(record + 8) : 1;
}

/** @brief 戻り値を残し、引数・局所変数・管理領域を解放する。
 *
 * @details
 * RETは宣言幅の戻り値を呼出し元の引数位置へ移し、フレームを解放する。再帰と値コピーに対応する。
 * 宣言幅ちょうどの一時値を先頭からコピーする。Noneの戻り値は従来通り1スロット。
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
    --depth_;
}

/** @brief 外部へ渡せる値か検査する。@param value 対象。@return 有効ならtrue。 */
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
    const char *data = nullptr;
    size_t length = 0;
    return stringBytes(value, data, length);
}

/** @brief 単一IDのスタブへスタック上の引数を渡す。@param id 呼出しID。
 * @details
 * CALL_NATIVEは単一IDでNativeApiの表を引く。
 * メソッドはselfのObjectハンドルを先頭に積み、コンストラクターはselfを受け取らない。
 * 引数は読取専用、結果は別領域を未初期化で用意し、成功後だけ引数と置換する。BPは変更しない。
 * スタブは各引数型・classId・ハンドルの有効性を検査し、全結果を宣言順に設定する責任を持つ。
 * コールバックはloop()で短時間に同期実行し、VM再入は禁止。
 * 未登録・ID欠落はエラー14、ABI不一致は15。スタブの失敗は返却されたErrorで停止する。
 * 生成表はdevice_api.generated.cpp、機器操作はdevice_native.cppが担当する。
 * NativePendingは待機通知で、PCと引数を保って次のtickで同じ命令を再試行する。
 * 待機時は結果を返さず、バックエンドが処理の進捗を保持する。成功した呼出しもloop()へ戻し、
 * 1回のtickで複数のI2C操作や長い転送を連続実行しない。instructionsは再試行回数も数える。
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
    std::unique_ptr<Value[]> results(new (std::nothrow) Value[entry.resultSlots]);
    if (!results) {
        fail(Error::Memory);
        return;
    }
    NativeCall call{*this, stack_.get() + first, results.get(), entry.argumentSlots,
        entry.resultSlots, nativeApi_->context};
    nativeActive_ = true;
    const Error result = entry.invoke(call);
    yielded_ = true;
    if (result == Error::NativePending) {
        pc = errorPc;
        return;
    }
    if (result != Error::None) {
        fail(result);
        return;
    }
    for (uint32_t index = 0; index < entry.resultSlots; ++index) {
        if (!validValue(results[index])) {
            fail(Error::Type);
            return;
        }
    }
    std::memcpy(stack_.get() + first, results.get(), entry.resultSlots * sizeof(Value));
    sp = first + entry.resultSlots;
}

/** @brief 定数または動的Stringの長さ付き参照を得る。@param value 値。@param data 出力。@param length バイト長。@return 有効ならtrue。
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

/** @brief 生存スロットを走査して空き文字列領域を確保する。@param length バイト長。@param value 結果。@param bytes 書込先。@return 成功時true。
 * @details
 * 動的文字列は64個・各1024バイトまで。確保前にglobalsと生存スタックを走査し、
 * 到達不能な文字列を回収する。
 */
bool BytecodeVM::allocateText(size_t length, Value &value, char *&bytes) {
    if (length > MaxStringLength) {
        return fail(Error::StringLimit);
    }
    bool used[MaxStrings]{};
    // pop済みの値はrootではない。連結では結果確保まで入力断片をスタックへ残す。
    for (uint32_t index = 0; index < globalCount + sp; ++index) {
        const Value &root = index < globalCount ? globals[index] : stack_[index - globalCount];
        if (root.type == Type::String && (root.address & 0x80000000U) != 0) {
            const uint32_t text = root.address & 0x7FFFFFFFU;
            if (text < MaxStrings) {
                used[text] = true;
            }
        }
    }
    uint32_t free = MaxStrings;
    for (uint32_t index = 0; index < MaxStrings; ++index) {
        if (!used[index]) {
            texts_[index].bytes.reset();
            texts_[index].length = 0;
            if (free == MaxStrings) {
                free = index;
            }
        }
    }
    if (free == MaxStrings) {
        return fail(Error::StringLimit);
    }
    Text &text = texts_[free];
    text.bytes.reset(new (std::nothrow) char[length + 1]);
    if (!text.bytes) {
        return fail(Error::Memory);
    }
    text.length = length;
    text.bytes[length] = '\0';
    value = Value{};
    value.type = Type::String;
    value.address = 0x80000000U | free;
    bytes = text.bytes.get();
    return true;
}

/** @brief 数値書式から表示用UTF-8文字列を作る。@param format 定数文字列ID。
 * @details
 * FORMAT_VALUE/BUILD_STRINGは表示引数だけの一時文字列を作る。
 * 書式は意味解析の数値書式（幅、精度、b/c/d/e/E/f/F/g/G/n/o/x/X/%）を扱う。
 * ロケールはC固定、無指定floatはfloat32を9桁のg形式で表示する（CPythonの最短表現とは異なる）。
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
        const char conversion = type == '%' ? 'f' : type == '\0' || type == 'n' ? 'g' : type;
        const char pattern[] = {'%', '.', '*', conversion, '\0'};
        const int digits = precision >= 0 ? precision : type == '\0' ? 9 : 6;
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

/** @brief 入力を生存させたまま連結し、その後1値へ置換する。@param count 断片数。 */
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

/** @brief 現在の範囲内で1命令を実行する。 */
void BytecodeVM::step() {
    errorPc = pc;
    if (pc < base_ || pc >= end_ || end_ > codeLength_) {
        fail(Error::Operand);
        return;
    }
    const uint8_t opcode = code_[pc];
    const uint32_t size = opcode == 0x03 || opcode == 0x04 ? 5 :
        opcode <= 0x09 || opcode == 0x14 || opcode == 0x19 || opcode == 0x1A || opcode == 0x1C || opcode == 0x1D ? 3 : 1;
    const uint8_t maximum = version_ == 3 ? 0x1D : version_ == 2 ? 0x19 : 0x18;
    if (opcode < 0x01 || opcode > maximum || pc + size > end_) {
        fail(Error::Format);
        return;
    }
    const uint16_t operand = size == 3 ? read16(code_ + pc + 1) : 0;
    const uint32_t bits = size == 5 ? read32(code_ + pc + 1) : 0;
    pc += size;
    ++instructions;
    Value value;
    switch (opcode) {
        case 0x01:
        case 0x02:
        case 0x05:
        case 0x06: {
            // Vec3/IMUValue等は宣言順に再帰展開した値スロット列であり、VMに構造体オブジェクトはない。
            // LOAD_G/LOAD_L・STORE_G/STORE_Lで葉をコピーする。
            const bool local = opcode == 0x01 || opcode == 0x05;
            if ((local && (depth_ == 0 || operand < 2 || bp + operand >= floor_)) ||
                (!local && operand >= globalCount)) {
                fail(Error::Operand);
                return;
            }
            Value &slot = local ? stack_[bp + operand] : globals[operand];
            if (opcode == 0x01 || opcode == 0x02) {
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
        case 0x03:
            value.type = Type::Integer;
            std::memcpy(&value.integer, &bits, sizeof bits);
            push(value);
            break;
        case 0x04:
            value.type = Type::Float;
            std::memcpy(&value.real, &bits, sizeof bits);
            if (!std::isfinite(value.real)) {
                fail(Error::NonFinite);
                return;
            }
            push(value);
            break;
        case 0x07:
        case 0x08:
        case 0x09: {
            bool condition = true;
            if (opcode != 0x07 && (!pop(value) || !truth(value, condition))) {
                return;
            }
            if (opcode == 0x07 || (opcode == 0x08 && !condition) || (opcode == 0x09 && condition)) {
                if (base_ + operand >= end_) {
                    fail(Error::Operand);
                    return;
                }
                pc = base_ + operand;
            }
            break;
        }
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            calculate(opcode);
            break;
        case 0x14:
            call(operand);
            break;
        case 0x15:
            ret();
            break;
        case 0x16:
            value.type = Type::None;
            push(value);
            break;
        case 0x17:
            pop(value);
            break;
        case 0x18:
            if (depth_ != 0 || sp != 0) {
                fail(Error::Operand);
                return;
            }
            // 診断用の値と登録状態は、次のstop/startまで保持する。
            state = State::Halted;
            break;
        case 0x19: {
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
        case 0x1A:
            callNative(operand);
            break;
        case 0x1B:
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
        case 0x1C:
            formatValue(operand);
            break;
        case 0x1D:
            buildString(operand);
            break;
    }
}

/** @brief 停止・通信処理へ制御を返すため実行量を制限する。@param budget 最大命令数。 */
void BytecodeVM::tick(uint32_t budget) {
    yielded_ = false;
    for (uint32_t count = 0; count < budget && state == State::Running && !yielded_; ++count) {
        step();
    }
}
