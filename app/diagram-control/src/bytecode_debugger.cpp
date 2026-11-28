/**
 * @file bytecode_debugger.cpp
 * @brief BLE／Wi-Fiデバッグの段階受信、opcode bit7の設定と停止時の値転送。
 * @details バイト単位の共通仕様はuroa/diagram/ts/ble-wifi/debug-protocol.tsを参照する。
 * lib/bleとlib/wifiはコピーだけ、main.cppはloop()の順序、bytecode_vm.cppは停止と値の直列化を担当する。
 * 行からPCへの変換はdiagramの生成器が担当する。
 * 要求・結果・Eventの固定領域はwire.protoの生成codecを使い、PC・CRC・停止IDの照合はここで行う。
 */
#include <uroa_wire.generated.h>
#include "bytecode_debugger.h"
#include "bytecode_format.h"
#include <cstring>
#include <new>

namespace {

    /**
     * @brief CRC-32/ISO-HDLC。
     * @param data 内容。
     * @param length 長さ。
     * @return CRC。
     */
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
}

/**
 * @brief 稼働中の依存先を保持する。
 * @param receiver 受信器。
 * @param vm VM。
 * @param transport 通信共通窓口。
 */
BytecodeDebugger::BytecodeDebugger(BytecodeReceiver &receiver, BytecodeVM &vm, ControlTransport &transport)
    : receiver_(receiver), vm_(vm), transport_(transport) {
}

/**
 * @brief setup()で4件の固定コピー領域を確保する。
 */
void BytecodeDebugger::begin() {
    requests_ = xQueueCreate(4, sizeof(Request));
}

/**
 * @brief 通信タスクからコピーする。
 * @param data 入力。
 * @param length 長さ。
 * @param session 受信元と接続世代。
 */
void BytecodeDebugger::enqueue(const uint8_t *data, size_t length, ControlSession session) {
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

/**
 * @brief 段階受信だけを破棄する。
 */
void BytecodeDebugger::cancelTransfer() {
    receiving_ = false;
    pointsTransfer_ = false;
    pending_.reset();
    count_ = 0;
    received_ = 0;
}

/**
 * @brief 検証済みの現イメージから命令列を取得する。
 * @param length 命令長。
 * @return 命令列。
 */
uint8_t *BytecodeDebugger::code(uint32_t &length) {
    BytecodeFormat::Image view;
    if (!BytecodeFormat::parseImage(receiver_.program.get(), receiver_.programLength, view)) {
        return nullptr;
    }
    length = view.codeLength;
    return receiver_.program.get() + (view.code - receiver_.program.get());
}

/**
 * @brief 同じイメージの命令先頭だけを元へ戻す。
 */
void BytecodeDebugger::clearFlags() {
    if (revision_ != receiver_.programRevision) {
        return;
    }
    uint32_t length = 0;
    uint8_t *bytes = code(length);
    for (uint32_t pc = 0; bytes && pc < length;) {
        const uint8_t opcode = bytes[pc] & BytecodeFormat::OpcodeMask;
        const uint32_t size = BytecodeFormat::instructionLength(opcode);
        if (size == 0 || size > length - pc) {
            return;
        }
        bytes[pc] = opcode;
        pc += size;
    }
}

/**
 * @brief Snapshotの寿命を終了させる。Stop後は古いデータを返さない。
 */
void BytecodeDebugger::invalidateSnapshot() {
    snapshot_.reset();
    snapshotLength_ = 0;
    snapshotCrc_ = 0;
    snapshotAttempted_ = false;
    snapshotError_ = Ok;
}

/**
 * @brief 接続変更・期限・要求をloop()で処理する。
 * @param enabled 現接続で処理可能。
 * @param session 受信元と接続世代。
 * @details 切断・Wi-Fi切替では命令フラグとステップ用停止点を失効させる。
 */
void BytecodeDebugger::process(bool enabled, ControlSession session) {
    const bool changedSession = session.source != session_.source || session.generation != session_.generation;
    if (enabled != enabled_ || changedSession || revision_ != receiver_.programRevision) {
        clearFlags();
        vm_.clearDebugPoints();
        cancelTransfer();
        invalidateSnapshot();
        if (changedSession) {
            requestId_ = 0;
            lastRequest_ = Request{};
        }
        enabled_ = enabled;
        session_ = session;
        revision_ = receiver_.programRevision;
        std::memset(event_, 0, sizeof event_);
        if (changedSession) {
            operation_ = 0;
            reply(Ok);
        }
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
        if (enabled_ && request.session.source == session_.source &&
            request.session.generation == session_.generation) {
            handle(request);
        }
    }
}

/**
 * @brief 全PCの順序・命令境界を確認した後だけフラグを全置換する。
 * @return 成功時true。
 * @details
 * 受信器のCRCはフラグ設定前のイメージを識別する。変更するのは検証済み命令先頭のbit7だけで、
 * プログラム世代は変えない。ステップ用の転送ではVMの独立した停止点表へ設定する。
 */
bool BytecodeDebugger::applyBreakpoints() {
    uint32_t length = 0;
    uint8_t *bytes = code(length);
    if (!bytes) {
        return false;
    }
    uint32_t selected = 0;
    for (uint32_t pc = 0; pc < length;) {
        const uint8_t opcode = bytes[pc] & BytecodeFormat::OpcodeMask;
        const uint32_t size = BytecodeFormat::instructionLength(opcode);
        if (size == 0 || size > length - pc || (selected < count_ && pending_[selected] < pc)) {
            return false;
        }
        if (selected < count_ && pending_[selected] == pc) {
            ++selected;
        }
        pc += size;
    }
    if (selected != count_ || !transport_.isCurrent(session_)) {
        return false;
    }
    if (pointsTransfer_) {
        return vm_.configureDebugPoints(pending_.get(), count_, bytes, length);
    }
    clearFlags();
    for (uint32_t index = 0; index < count_; ++index) {
        bytes[pending_[index]] |= BytecodeFormat::BreakpointFlag;
    }
    return true;
}

/**
 * @brief 受信器が全件検証した行PCとステップPCを開始前に設定する。
 * @return 設定成功。
 */
bool BytecodeDebugger::installDeployment() {
    uint32_t length = 0;
    uint8_t *bytes = code(length);
    if (!bytes || !transport_.isCurrent(session_) ||
        !vm_.configureDebugPoints(receiver_.stepPoints, receiver_.stepCount, bytes, length)) {
        return false;
    }
    clearFlags();
    for (uint32_t index = 0; index < receiver_.breakpointCount; ++index) {
        bytes[receiver_.breakpoints[index]] |= BytecodeFormat::BreakpointFlag;
    }
    return true;
}

/**
 * @brief 要求ACKをReadへ公開する。
 * @param code 結果。
 * @param value 操作別値。
 * @param extra 操作別値。
 */
void BytecodeDebugger::reply(Code code, uint32_t value, uint32_t extra) {
    const uroa::wire::DebugResult packet{1, operation_, requestId_, static_cast<uint8_t>(code),
        static_cast<uint8_t>(vm_.state), 0, receiver_.programRevision, value, extra};
    uroa::wire::writeDebugResult(result_, packet);
    transport_.publishDebugResult(result_, sizeof result_, session_);
}

/**
 * @brief 要求を順番に処理する。
 * @param request 現接続の固定コピー。
 * @details 再開・ステップ要求は停止IDと世代を検証し、VMへ委譲する。
 */
void BytecodeDebugger::handle(const Request &request) {
    const uint8_t *data = request.bytes;
    const auto header = uroa::wire::readDebugHeader(data);
    const uint16_t id = header.request;
    if (id == requestId_ && id != 0 && request.length == lastRequest_.length &&
        std::memcmp(data, lastRequest_.bytes, request.length) == 0) {
        transport_.publishDebugResult(result_, sizeof result_, session_);
        return;
    }
    if (id == 0 || id <= requestId_) {
        return;
    }
    requestId_ = id;
    operation_ = header.op;
    lastRequest_ = request;
    if (header.version != 1 || operation_ < 1 || operation_ > 13) {
        reply(Format);
        return;
    }
    if (operation_ == 4) {
        if (request.length != 8 || header.revision != 0) {
            reply(Format);
        }
        else {
            reply(Ok, receiver_.programCrc32, receiver_.programLength);
        }
        return;
    }
    if (!receiver_.program || header.revision != receiver_.programRevision) {
        reply(Stale);
        return;
    }
    if (operation_ == 5) {
        if (request.length != 16) {
            reply(Format);
            return;
        }
        const auto chunk = uroa::wire::readDebugChunk(data);
        const uint32_t offset = chunk.offset;
        if (vm_.state != BytecodeVM::State::Paused || !snapshot_ || chunk.stopId != stopId_) {
            reply(Stale);
        }
        else if (offset >= snapshotLength_) {
            reply(Range);
        }
        else {
            const uint32_t size = snapshotLength_ - offset < 20 ? snapshotLength_ - offset : 20;
            transport_.publishDebugData(snapshot_.get() + offset, size, session_);
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
    if (operation_ >= 10) {
        if (request.length != 12) {
            reply(Format);
            return;
        }
        const uint32_t selectedStop = uroa::wire::readDebugExecute(data).stopId;
        if (receiving_ || vm_.state != BytecodeVM::State::Paused || !snapshot_ || selectedStop != stopId_) {
            reply(Stale);
            return;
        }
        const auto action = static_cast<BytecodeVM::DebugAction>(operation_ - 10);
        if (!vm_.resumeDebug(action)) {
            reply(Invalid);
            return;
        }
        invalidateSnapshot();
        reply(Ok, selectedStop);
        // 同じ周回で再停止しても新しいstopIdを発行し、旧Snapshotと区別する。
        publishState();
        return;
    }
    if (vm_.state == BytecodeVM::State::Running) {
        reply(Busy);
        return;
    }
    const uint8_t transferOperation = operation_ >= 7 ? operation_ - 6 : operation_;
    if (transferOperation == 1) {
        if (request.length != uroa::wire::DebugBeginLayout::ByteLength) {
            reply(Invalid);
            return;
        }
        const auto begin = uroa::wire::readDebugBegin(data);
        if (begin.imageCrc != receiver_.programCrc32) {
            reply(Invalid);
            return;
        }
        const uint32_t count = begin.count;
        if (count > (operation_ == 7 ? 65536U : 4096U)) {
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
        pointsTransfer_ = operation_ == 7;
        pending_ = std::move(pending);
        count_ = count;
        expectedCrc_ = begin.crc;
        receiving_ = true;
        lastReceive_ = millis();
        reply(Ok);
        return;
    }
    if (!receiving_ || pointsTransfer_ != (operation_ >= 7)) {
        reply(Order);
        return;
    }
    if (transferOperation == 2) {
        if (request.length < 14 || request.length > 20 || (request.length - 12) % 2 != 0) {
            reply(Format);
            return;
        }
        const uint32_t count = (request.length - 12) / 2;
        if (uroa::wire::readDebugData(data).index != received_ || count > count_ - received_) {
            reply(Order);
            return;
        }
        for (uint32_t index = 0; index < count; ++index) {
            const uint16_t pc = uroa::wire::readPc(
                data + uroa::wire::DebugDataLayout::ByteLength + index * uroa::wire::PcLayout::ByteLength)
                                    .value;
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

/**
 * @brief 状態変化とRunningの進捗を通知する。Event v2の契約はWebのdebug-protocol.ts先頭を参照する。
 * @details
 * PausedはVMだけを止め、既存の出力・補間は維持する。既存Stop・切断・通信期限の停止は優先する。
 * 停止値は一度だけコピーして保持し、再開時はinvalidateSnapshot()で失効させる。
 */
void BytecodeDebugger::publishState() {
    if (!enabled_) {
        return;
    }
    const uint32_t now = millis();
    if (vm_.state == BytecodeVM::State::Running && uroa::wire::readDebugEvent(event_).version == 2 &&
        uroa::wire::readDebugEvent(event_).state == uint8_t(vm_.state) &&
        uroa::wire::readDebugEvent(event_).revision == receiver_.programRevision &&
        uint32_t(now - lastProgressMs_) < 1000) {
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
            snapshotLength_ = vm_.debugSnapshotLength();
            if (snapshotLength_ != 0) {
                snapshot_.reset(new (std::nothrow) uint8_t[snapshotLength_]);
            }
            if (snapshotLength_ == 0) {
                snapshotError_ = Invalid;
            }
            else if (!snapshot_) {
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
    const bool paused = vm_.state == BytecodeVM::State::Paused;
    const uroa::wire::DebugEvent packet{2, static_cast<uint8_t>(vm_.state), static_cast<uint8_t>(snapshotError_),
        static_cast<uint8_t>(vm_.state == BytecodeVM::State::Error ? uint8_t(vm_.error) : 0),
        paused                                      ? stopId_
            : vm_.state == BytecodeVM::State::Error ? vm_.errorPc
                                                    : 0,
        receiver_.programRevision, paused ? snapshotLength_ : vm_.instructions, paused ? snapshotCrc_ : vm_.pc};
    uint8_t event[uroa::wire::DebugEventLayout::ByteLength];
    uroa::wire::writeDebugEvent(event, packet);
    if (std::memcmp(event_, event, sizeof event_) != 0) {
        std::memcpy(event_, event, sizeof event_);
        lastProgressMs_ = now;
        transport_.publishDebugEvent(event_, sizeof event_, session_);
    }
}
