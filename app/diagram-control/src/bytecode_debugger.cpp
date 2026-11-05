/** @file bytecode_debugger.cpp
 * @brief BLEデバッグの段階受信、opcode bit7の設定と停止時の値転送。
 * @details バイト単位の共通仕様はuroa/diagram/ts/ble-wifi/debug-protocol.tsを参照する。
 * lib/bleはコピーだけ、main.cppはloop()の順序、bytecode_vm.cppは停止と値の直列化を担当する。
 * 受信器のCRCはフラグ設定前のイメージを識別する。変更するのは検証済み命令先頭のbit7だけで、
 * プログラム世代は変えない。切断・Wi-Fi切替ではフラグを戻す。動的文字列本体は送らない。
 * PausedはVMだけを止め、既存の出力・補間は維持する。既存Stop・切断・通信期限の停止は優先する。
 * 再開・ステップ・行番号からPCへの変換はこの通信実装に含めない。
 */
#include "bytecode_debugger.h"
#include <cstring>
#include <new>

namespace {
    /** @brief 非整列little endianを読む。@param data 2バイト。@return 値。 */
    uint16_t read16(const uint8_t *data) {
        return uint16_t(data[0]) | uint16_t(data[1]) << 8;
    }
    /** @brief 非整列little endianを読む。@param data 4バイト。@return 値。 */
    uint32_t read32(const uint8_t *data) {
        return uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
    }
    /** @brief little endianで書く。@param data 出力。@param value 値。 */
    void write32(uint8_t *data, uint32_t value) {
        for (size_t index = 0; index < 4; ++index) {
            data[index] = uint8_t(value >> (8 * index));
        }
    }
    /** @brief CRC-32/ISO-HDLC。@param data 内容。@param length 長さ。@return CRC。 */
    uint32_t crc32(const uint8_t *data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t index = 0; index < length; ++index) {
            crc ^= data[index];
            for (size_t bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
        }
        return crc ^ 0xFFFFFFFF;
    }
    /** @brief フラグ除去後の命令長。@param opcode 0x01〜0x1D。@return バイト数。 */
    uint32_t instructionLength(uint8_t opcode) {
        return opcode == 0x03 || opcode == 0x04 ? 5 :
            opcode <= 0x09 || opcode == 0x14 || opcode == 0x19 || opcode == 0x1A || opcode == 0x1C || opcode == 0x1D ? 3 : 1;
    }
}

/** @brief 稼働中の依存先を保持する。@param receiver 受信器。@param vm VM。@param ble BLE。 */
BytecodeDebugger::BytecodeDebugger(BytecodeReceiver &receiver, BytecodeVM &vm, uroa::BleTransport &ble)
    : receiver_(receiver), vm_(vm), ble_(ble) {
}

/** @brief setup()で4件の固定コピー領域を確保する。 */
void BytecodeDebugger::begin() {
    requests_ = xQueueCreate(4, sizeof(Request));
}

/** @brief BLEタスクからコピーする。@param data 入力。@param length 長さ。@param session 接続世代。 */
void BytecodeDebugger::enqueue(const uint8_t *data, size_t length, uint32_t session) {
    if (!requests_ || !data || length < 8 || length > 20) {
        overflow_.store(true);
        return;
    }
    Request request;
    request.session = session;
    request.length = uint8_t(length);
    std::memcpy(request.bytes, data, length);
    if (xQueueSend(requests_, &request, 0) != pdTRUE) {
        overflow_.store(true);
    }
}

/** @brief 段階受信だけを破棄する。 */
void BytecodeDebugger::cancelTransfer() {
    receiving_ = false;
    pending_.reset();
    count_ = 0;
    received_ = 0;
}

/** @brief 検証済みの現イメージから命令列を取得する。@param length 命令長。@return 命令列。 */
uint8_t *BytecodeDebugger::code(uint32_t &length) {
    if (!receiver_.program || receiver_.programLength < 16) {
        return nullptr;
    }
    uint8_t *image = receiver_.program.get();
    const uint16_t version = read16(image + 2);
    const size_t offset = (version == 1 ? 16 : version == 2 ? 24 : 56) +
        size_t(read16(image + 6)) * (version == 3 ? 12 : 8);
    length = read32(image + 8);
    if (offset > receiver_.programLength || length > receiver_.programLength - offset) {
        return nullptr;
    }
    return image + offset;
}

/** @brief 同じイメージの命令先頭だけを元へ戻す。 */
void BytecodeDebugger::clearFlags() {
    if (revision_ != receiver_.programRevision) {
        return;
    }
    uint32_t length = 0;
    uint8_t *bytes = code(length);
    for (uint32_t pc = 0; bytes && pc < length;) {
        const uint8_t opcode = bytes[pc] & 0x7F;
        bytes[pc] = opcode;
        pc += instructionLength(opcode);
    }
}

/** @brief Snapshotの寿命を終了させる。Stop後は古いデータを返さない。 */
void BytecodeDebugger::invalidateSnapshot() {
    snapshot_.reset();
    snapshotLength_ = 0;
    snapshotCrc_ = 0;
    snapshotAttempted_ = false;
    snapshotError_ = Ok;
}

/** @brief 接続変更・期限・要求をloop()で処理する。@param enabled BLE制御中。@param session 接続世代。 */
void BytecodeDebugger::process(bool enabled, uint32_t session) {
    if (enabled != enabled_ || session != session_ || revision_ != receiver_.programRevision) {
        clearFlags();
        cancelTransfer();
        invalidateSnapshot();
        if (session != session_) {
            requestId_ = 0;
            lastRequest_ = Request{};
        }
        enabled_ = enabled;
        session_ = session;
        revision_ = receiver_.programRevision;
        std::memset(event_, 0, sizeof event_);
    }
    if (overflow_.exchange(false)) {
        cancelTransfer();
        reply(Busy);
    }
    if (receiving_ && uint32_t(millis() - lastReceive_) > 5000) {
        cancelTransfer();
        reply(Timeout);
    }
    Request request;
    for (size_t index = 0; requests_ && index < 4 && xQueueReceive(requests_, &request, 0) == pdTRUE; ++index) {
        if (enabled_ && request.session == session_) {
            handle(request);
        }
    }
}

/** @brief 全PCの順序・命令境界を確認した後だけフラグを全置換する。@return 成功時true。 */
bool BytecodeDebugger::applyBreakpoints() {
    uint32_t length = 0;
    uint8_t *bytes = code(length);
    if (!bytes) {
        return false;
    }
    uint32_t selected = 0;
    for (uint32_t pc = 0; pc < length;) {
        const uint8_t opcode = bytes[pc] & 0x7F;
        const uint32_t size = instructionLength(opcode);
        if (opcode < 1 || opcode > 0x1D || size > length - pc ||
            (selected < count_ && pending_[selected] < pc)) {
            return false;
        }
        if (selected < count_ && pending_[selected] == pc) {
            ++selected;
        }
        pc += size;
    }
    if (selected != count_ || !ble_.connected.load() || ble_.generation.load() != session_) {
        return false;
    }
    clearFlags();
    for (uint32_t index = 0; index < count_; ++index) {
        bytes[pending_[index]] |= 0x80;
    }
    return true;
}

/** @brief 要求ACKをReadへ公開する。@param code 結果。@param value 操作別値。@param extra 操作別値。 */
void BytecodeDebugger::reply(Code code, uint32_t value, uint32_t extra) {
    std::memset(result_, 0, sizeof result_);
    result_[0] = 1;
    result_[1] = operation_;
    result_[2] = uint8_t(requestId_);
    result_[3] = uint8_t(requestId_ >> 8);
    result_[4] = code;
    result_[5] = uint8_t(vm_.state);
    write32(result_ + 8, receiver_.programRevision);
    write32(result_ + 12, value);
    write32(result_ + 16, extra);
    ble_.publishDebugResult(result_, sizeof result_);
}

/** @brief 要求を順番に処理する。@param request 現接続の固定コピー。 */
void BytecodeDebugger::handle(const Request &request) {
    const uint8_t *data = request.bytes;
    const uint16_t id = read16(data + 2);
    if (id == requestId_ && id != 0 && request.length == lastRequest_.length &&
        std::memcmp(data, lastRequest_.bytes, request.length) == 0) {
        ble_.publishDebugResult(result_, sizeof result_);
        return;
    }
    if (id == 0 || id <= requestId_) {
        return;
    }
    requestId_ = id;
    operation_ = data[1];
    lastRequest_ = request;
    if (data[0] != 1 || operation_ < 1 || operation_ > 6) {
        reply(Format);
        return;
    }
    if (operation_ == 4) {
        if (request.length != 8 || read32(data + 4) != 0) {
            reply(Format);
        }
        else {
            reply(Ok, receiver_.programCrc32, receiver_.programLength);
        }
        return;
    }
    if (!receiver_.program || read32(data + 4) != receiver_.programRevision) {
        reply(Stale);
        return;
    }
    if (operation_ == 5) {
        if (request.length != 16) {
            reply(Format);
            return;
        }
        const uint32_t offset = read32(data + 12);
        if (vm_.state != BytecodeVM::State::Paused || !snapshot_ || read32(data + 8) != stopId_) {
            reply(Stale);
        }
        else if (offset >= snapshotLength_) {
            reply(Range);
        }
        else {
            const uint32_t size = snapshotLength_ - offset < 20 ? snapshotLength_ - offset : 20;
            ble_.publishDebugData(snapshot_.get() + offset, size);
            reply(Ok, offset, size);
        }
        return;
    }
    if (operation_ == 6) {
        if (request.length != 8) {
            reply(Format);
            return;
        }
        cancelTransfer();
        reply(Ok);
        return;
    }
    if (vm_.state == BytecodeVM::State::Running) {
        reply(Busy);
        return;
    }
    if (operation_ == 1) {
        if (request.length != 20 || read32(data + 16) != receiver_.programCrc32) {
            reply(Invalid);
            return;
        }
        const uint32_t count = read32(data + 8);
        if (count > 4096) {
            reply(Range);
            return;
        }
        std::unique_ptr<uint16_t[]> pending;
        if (count) {
            pending.reset(new (std::nothrow) uint16_t[count]);
            if (!pending) {
                reply(Memory);
                return;
            }
        }
        cancelTransfer();
        pending_ = std::move(pending);
        count_ = count;
        expectedCrc_ = read32(data + 12);
        receiving_ = true;
        lastReceive_ = millis();
        reply(Ok);
        return;
    }
    if (!receiving_) {
        reply(Order);
        return;
    }
    if (operation_ == 2) {
        if (request.length < 14 || request.length > 20 || (request.length - 12) % 2 != 0) {
            reply(Format);
            return;
        }
        const uint32_t count = (request.length - 12) / 2;
        if (read32(data + 8) != received_ || count > count_ - received_) {
            reply(Order);
            return;
        }
        for (uint32_t index = 0; index < count; ++index) {
            const uint16_t pc = read16(data + 12 + index * 2);
            if (received_ && pc <= pending_[received_ - 1]) {
                cancelTransfer();
                reply(Order);
                return;
            }
            pending_[received_++] = pc;
        }
        lastReceive_ = millis();
        reply(Ok, received_);
        return;
    }
    if (request.length != 8 || received_ != count_) {
        reply(Order);
        return;
    }
    // ESP32はlittle endianだが、通信CRCも明示的な2バイト値の列として計算する。
    const uint32_t actualCrc = crc32(reinterpret_cast<const uint8_t *>(pending_.get()), count_ * 2);
    const uint32_t accepted = count_;
    const Code result = actualCrc != expectedCrc_ ? Crc : applyBreakpoints() ? Ok : Invalid;
    cancelTransfer();
    reply(result, result == Ok ? accepted : 0);
}

/** @brief 状態変化だけを通知する。Snapshotは最大16408バイトで、チャンク読取り中も不変。 */
void BytecodeDebugger::publishState() {
    if (!enabled_) {
        return;
    }
    if (vm_.state != BytecodeVM::State::Paused) {
        invalidateSnapshot();
    }
    else if (!snapshotAttempted_) {
        snapshotAttempted_ = true;
        if (stopId_ == UINT32_MAX) {
            snapshotError_ = Range;
        }
        else {
            ++stopId_;
            snapshotLength_ = 24 + 8 * (vm_.globalCount + vm_.sp);
            snapshot_.reset(new (std::nothrow) uint8_t[snapshotLength_]);
            if (!snapshot_) {
                snapshotLength_ = 0;
                snapshotError_ = Memory;
            }
            else if (!vm_.copyDebugSnapshot(snapshot_.get(), snapshotLength_)) {
                snapshot_.reset();
                snapshotLength_ = 0;
                snapshotError_ = Invalid;
            }
            else {
                snapshotCrc_ = crc32(snapshot_.get(), snapshotLength_);
            }
        }
    }
    uint8_t event[20]{};
    event[0] = 1;
    event[1] = uint8_t(vm_.state);
    event[2] = snapshotError_;
    write32(event + 4, vm_.state == BytecodeVM::State::Paused ? stopId_ : 0);
    write32(event + 8, receiver_.programRevision);
    write32(event + 12, snapshotLength_);
    write32(event + 16, snapshotCrc_);
    if (std::memcmp(event_, event, sizeof event_) != 0) {
        std::memcpy(event_, event, sizeof event_);
        ble_.publishDebugEvent(event_, sizeof event_);
    }
}
