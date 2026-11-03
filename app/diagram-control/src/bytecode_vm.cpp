/** @file bytecode_vm.cpp
 * @brief 数値演算・分岐・関数フレームを持つPythonサブセットVM。
 *
 * @details
 * # PythonサブセットVM
 *
 * 2026-11-03。`BytecodeVM`は受信器が検証したバイナリversion=1の24命令を実行する。`include/bytecode_vm.h`に公開状態・型・上限、`src/bytecode_vm.cpp`に実装を置く。
 *
 * 機器API、print、文字列・リスト・クラスは実装しない。VMからサーボ等を操作する命令はない。
 *
 * ## 所有権と確認範囲
 *
 * 以下は2026-11-03のVM実装時の確認範囲。今回のコメント移動ではビルド・テストを行わない。
 *
 * 今回のビルド対象はDevKitの`esp32dev`のみ。ユーザー指定によりテスト・他ボードのビルド・Uploadは行わない。実BLEとVMの実行結果はビルドとは別に未確認とする。
 */
#include "bytecode_vm.h"
#include <cstring>
#include <cmath>
#include <new>
#include <limits.h>

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

/** @brief エラー状態へ移る。@param code 理由。@return false。 */
bool BytecodeVM::fail(Error code) {
    error = code;
    state = State::Error;
    return false;
}

/** @brief 借用バッファを参照しない状態へ戻す。大域変数は保持する。 */
void BytecodeVM::stop() {
    if (state != State::Idle) {
        state = State::Stopped;
    }
    code_ = nullptr;
    table_ = nullptr;
    stack_.reset();
    depth_ = 0;
}

/** @brief 検証済みイメージの開始を準備する。@param image 全体。@param length 長さ。@return 成功時true。
 *
 * @details
 * 大域変数と追加の局所変数は未初期化で用意し、LOADでエラーにする。
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
    if (image == nullptr || length < 16 || read16(image) != 0xB1C0 || read16(image + 2) != 1 ||
        read16(image + 12) != 0 || read16(image + 14) != 0) {
        return fail(Error::Format);
    }
    globalCount = read16(image + 4);
    functionCount_ = read16(image + 6);
    codeLength_ = read32(image + 8);
    const size_t offset = 16 + size_t(functionCount_) * 8;
    if (codeLength_ == 0 || codeLength_ > 65536 || offset + codeLength_ != length) {
        return fail(Error::Format);
    }
    if (globalCount > MaxGlobals) {
        return fail(Error::Limit);
    }
    table_ = image + 16;
    code_ = image + offset;
    end_ = functionCount_ == 0 ? codeLength_ : read16(table_);
    uint32_t previousEnd = end_;
    if (end_ == 0 || end_ > codeLength_) {
        return fail(Error::Format);
    }
    for (size_t index = 0; index < functionCount_; ++index) {
        const uint8_t *record = table_ + index * 8;
        const uint32_t begin = read16(record);
        const uint32_t end = begin + read16(record + 2);
        if (begin != previousEnd || end <= begin || end > codeLength_) {
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
 */
void BytecodeVM::call(uint16_t entry) {
    size_t low = 0;
    size_t high = functionCount_;
    while (low < high) {
        const size_t middle = (low + high) / 2;
        if (read16(table_ + middle * 8) < entry) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if (low == functionCount_ || read16(table_ + low * 8) != entry) {
        fail(Error::Operand);
        return;
    }
    const uint8_t *record = table_ + low * 8;
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
    stack_[base].type = Type::ReturnPc;
    stack_[base].address = pc;
    stack_[base + 1].type = Type::SavedBp;
    stack_[base + 1].address = bp;
    for (uint32_t index = 0; index < locals; ++index) {
        stack_[base + 2 + args + index] = Value{};
    }
    frames_[depth_++] = {base_, end_, floor_};
    bp = base;
    base_ = entry;
    end_ = entry + read16(record + 2);
    floor_ = base + 2 + args + locals;
    sp = floor_;
    pc = entry;
}

/** @brief 戻り値を残し、引数・局所変数・管理領域を解放する。
 *
 * @details
 * RETはトップの戻り値を退避し、フレームを解放してPC・BP・呼出元の命令範囲を復元し、戻り値を1個pushする。再帰呼出しに対応する。
 */
void BytecodeVM::ret() {
    Value result;
    if (depth_ == 0 || bp + 1 >= floor_ || stack_[bp].type != Type::ReturnPc ||
        stack_[bp + 1].type != Type::SavedBp) {
        fail(Error::Return);
        return;
    }
    if (!pop(result)) {
        return;
    }
    const Frame caller = frames_[depth_ - 1];
    const uint32_t returnPc = stack_[bp].address;
    const uint32_t oldBp = stack_[bp + 1].address;
    if (returnPc < caller.base || returnPc >= caller.end || oldBp > bp) {
        fail(Error::Return);
        return;
    }
    sp = bp;
    bp = oldBp;
    base_ = caller.base;
    end_ = caller.end;
    floor_ = caller.floor;
    pc = returnPc;
    --depth_;
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
    const uint32_t size = opcode == 0x03 || opcode == 0x04 ? 5 : opcode <= 0x09 || opcode == 0x14 ? 3 : 1;
    if (opcode < 0x01 || opcode > 0x18 || pc + size > end_) {
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
            state = State::Halted;
            break;
    }
}

/** @brief 停止・通信処理へ制御を返すため実行量を制限する。@param budget 最大命令数。 */
void BytecodeVM::tick(uint32_t budget) {
    for (uint32_t count = 0; count < budget && state == State::Running; ++count) {
        step();
    }
}
