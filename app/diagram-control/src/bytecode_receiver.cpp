/**
 * @file bytecode_receiver.cpp
 * @brief 一括転送version=3を蓄積しloop()でコード・PCを検証する。
 * @details 通信の中心契約はuroa/diagram/ts/ble-wifi/bytecode-transfer.ts先頭。
 * 受信領域の確保・コピーは通信タスクでmutex排他し、命令検証・確定はloop()で行う。
 * 確定コードはRAMへ保持し、Flash保存は行わない。
 * フレーム・Bundle・Resultの固定領域はwire.protoの生成codecを使う。
 * コード本体の命令検証・受信位置・所有権と検証後の確定を扱う。通信CRCと自動再送は持たない。
 */
#include <uroa_wire.generated.h>
#include "bytecode_receiver.h"
#include "bytecode_format.h"
#include <cstring>
#include <new>

using BytecodeFormat::Opcode;

namespace {
    using namespace uroa::wire::TransferCode;
    using namespace uroa::wire::TransferOperation;
    using namespace uroa::wire::TransferState;

    /**
     * @brief 長さ付きバイト列がUnicodeスカラー値のUTF-8表現か検証する。
     * @param data 文字列の先頭。NUL終端は要求しない。
     * @param length バイト長。0も許可する。
     * @return 過長表現・不正継続・サロゲート・範囲外がなければtrue。
     */
    bool validUtf8(const uint8_t *data, size_t length) {
        size_t position = 0;
        while (position < length) {
            const uint8_t first = data[position++];
            if (first <= 0x7F) {
                continue;
            }
            size_t continuation = 0;
            uint32_t codePoint = 0;
            uint32_t minimum = 0;
            if (first >= 0xC2 && first <= 0xDF) {
                continuation = 1;
                codePoint = first & 0x1F;
                minimum = 0x80;
            }
            else if (first >= 0xE0 && first <= 0xEF) {
                continuation = 2;
                codePoint = first & 0x0F;
                minimum = 0x800;
            }
            else if (first >= 0xF0 && first <= 0xF4) {
                continuation = 3;
                codePoint = first & 0x07;
                minimum = 0x10000;
            }
            else {
                return false;
            }
            if (continuation > length - position) {
                return false;
            }
            for (size_t index = 0; index < continuation; ++index) {
                const uint8_t next = data[position++];
                if ((next & 0xC0) != 0x80) {
                    return false;
                }
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
            if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 非整列のlittle endian値を読む。
     * @param p 2バイトの先頭。
     * @return uint16値。
     */
    uint16_t read16(const uint8_t *p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }

    /**
     * @brief 非整列のlittle endian値を読む。
     * @param p 4バイトの先頭。
     * @return uint32値。
     */
    uint32_t read32(const uint8_t *p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }

}

/**
 * @brief mutexと配送先を準備する。
 * @param publish Result配送。
 * @param current 世代照合。
 */
void BytecodeReceiver::begin(const std::function<void(const uint8_t *, size_t, const ControlSession &)> &publish,
    const std::function<bool(const ControlSession &)> &current) {
    publish_ = publish;
    current_ = current;
    lock_ = xSemaphoreCreateMutex();
}

/**
 * @brief 最初のエラーを保持する。
 * @param code 結果。
 */
void BytecodeReceiver::reject(Code code) {
    if (error_ == Ok) {
        error_ = code;
    }
    pending_.reset();
    receiving_ = false;
    sealed_ = false;
    state_ = Failed;
    dirty_ = true;
}

/**
 * @brief 通信タスクでヘッダーを検査して専用領域へコピーする。
 * @param data フレーム。
 * @param length 長さ。
 * @param session 接続世代。
 * @details BEGINで最大MaxBundleまで確保する。DATAは順序・範囲だけを検査し、VMを操作しない。
 * mutex内のメモリ確保失敗もResultで保持する。キュー4件という受信容量の制限は使用しない。
 * 新旧コードとPC一覧を同時保持できるRAMが必要。確保失敗はエラーにし、部分コードを実行しない。
 */
void BytecodeReceiver::enqueue(const uint8_t *data, size_t length, ControlSession session) {
    if (!lock_ || !data || !current_ || !current_(session)) {
        return;
    }
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (session.source != session_.source || session.generation != session_.generation) {
        pending_.reset();
        session_ = session;
        transferId_ = 0;
        error_ = Ok;
        receiving_ = false;
        sealed_ = false;
        state_ = Idle;
    }
    if (length < uroa::wire::TransferHeaderLayout::ByteLength || length > MaxFrame ||
        data[uroa::wire::TransferHeaderLayout::version] != uroa::wire::TransferHeaderLayout::versionDefault) {
        reject(Format);
        xSemaphoreGive(lock_);
        return;
    }
    const auto header = uroa::wire::readTransferHeader(data);
    const uint8_t op = header.op;
    const uint16_t id = header.transfer;
    const uint16_t seq = header.sequence;
    const uint32_t pos = header.position;
    if (op == Begin && id > transferId_ && seq == 1 && !deploymentPending) {
        pending_.reset();
        transferId_ = id;
        sequence_ = 1;
        operation_ = Begin;
        error_ = Ok;
        received_ = 0;
        total_ = pos;
        state_ = Receiving;
        sealed_ = false;
        receiving_ = false;
        if (length != uroa::wire::TransferHeaderLayout::ByteLength ||
            pos < uroa::wire::BundleHeaderLayout::ByteLength || pos > MaxBundle) {
            reject(Range);
        }
        else {
            pending_.reset(new (std::nothrow) uint8_t[pos]);
            if (!pending_) {
                reject(Memory);
            }
            else {
                receiving_ = true;
                lastReceive_ = millis();
            }
        }
        dirty_ = true;
        xSemaphoreGive(lock_);
        return;
    }
    // 古い転送と同じCOMMITの重複は処理せず、確定した結果を維持する。
    if (id != transferId_ || error_ != Ok || (!receiving_ && op == Commit) || sealed_) {
        xSemaphoreGive(lock_);
        return;
    }
    if (!receiving_ || seq != uint16_t(sequence_ + 1)) {
        reject(Order);
    }
    else {
        sequence_ = seq;
        operation_ = op;
        if (op == Data && length > uroa::wire::TransferHeaderLayout::ByteLength && pos == received_ &&
            length - uroa::wire::TransferHeaderLayout::ByteLength <= total_ - received_) {
            std::memcpy(pending_.get() + received_, data + uroa::wire::TransferHeaderLayout::ByteLength,
                length - uroa::wire::TransferHeaderLayout::ByteLength);
            received_ += length - uroa::wire::TransferHeaderLayout::ByteLength;
            lastReceive_ = millis();
        }
        else if (op == Commit && length == uroa::wire::TransferHeaderLayout::ByteLength && pos == total_ &&
            received_ == total_) {
            sealed_ = true;
            receiving_ = false;
        }
        else if (op == Abort && length == uroa::wire::TransferHeaderLayout::ByteLength && pos == 0) {
            reject(Start);
        }
        else {
            reject(Order);
        }
    }
    dirty_ = true;
    xSemaphoreGive(lock_);
}

/**
 * @brief 接続・期限・全体検証をloop()で処理する。
 * @param connected 接続。
 * @param session 世代。
 */
void BytecodeReceiver::process(bool connected, ControlSession session) {
    if (!lock_) {
        return;
    }
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (!connected || session.source != session_.source || session.generation != session_.generation) {
        pending_.reset();
        receiving_ = false;
        sealed_ = false;
        deploymentPending = false;
        session_ = session;
        transferId_ = 0;
        sequence_ = 0;
        error_ = Ok;
        state_ = Idle;
        operation_ = None;
        received_ = 0;
        total_ = 0;
        dirty_ = true;
    }
    else if (receiving_ && uint32_t(millis() - lastReceive_) > 15000) {
        reject(Timeout);
    }
    else if (sealed_) {
        sealed_ = false;
        const Code result = adopt();
        if (result != Ok) {
            reject(result);
        }
        else {
            deploymentPending = true;
        }
        dirty_ = true;
    }
    const bool publish = dirty_;
    xSemaphoreGive(lock_);
    if (publish) {
        publishResult();
    }
}

/**
 * @brief 検証済みコードとPC列を照合する。
 * @param image コード。
 * @param imageLength イメージ全長。
 * @param pcs little endian列。
 * @param count 個数。
 * @return 全一致。
 */
bool BytecodeReceiver::validatePcs(const uint8_t *image, size_t imageLength, const uint8_t *pcs, uint32_t count) {
    BytecodeFormat::Image view;
    if (!BytecodeFormat::parseImage(image, imageLength, view)) {
        return false;
    }
    const uint32_t length = view.codeLength;
    uint32_t selected = 0;
    for (uint32_t pc = 0; pc < length;) {
        if (selected < count && read16(pcs + selected * 2) < pc) {
            return false;
        }
        if (selected < count && read16(pcs + selected * 2) == pc) {
            if (selected && read16(pcs + selected * 2) <= read16(pcs + (selected - 1) * 2)) {
                return false;
            }
            ++selected;
        }
        const uint32_t size = BytecodeFormat::instructionLength(view.code[pc]);
        if (size == 0 || size > length - pc) {
            return false;
        }
        pc += size;
    }
    return selected == count;
}

/**
 * @brief 全件検証後だけコードとPC一覧の所有領域を置換する。
 * @return 検証結果。
 * @details 確定バッファ先頭へコードを移し、末尾PC列も同じ所有領域に保持する。
 */
BytecodeReceiver::Code BytecodeReceiver::adopt() {
    const uint8_t *p = pending_.get();
    // BEGINでヘッダーを収める全長を確認し、全バイト受信後にだけ呼ぶ。
    const auto header = uroa::wire::readBundleHeader(p);
    const uint32_t length = header.length;
    const uint32_t bpCount = header.breakpoints;
    const uint32_t pointsCount = header.points;
    const size_t pcOffset = uroa::wire::BundleHeaderLayout::ByteLength + size_t(length) + length % 2;
    if (header.signature != uroa::wire::BundleHeaderLayout::signatureDefault ||
        header.version != uroa::wire::BundleHeaderLayout::versionDefault || header.flags > 1 || length < 16 ||
        length > MaxLength || bpCount > uroa::wire::ProgramBundleLimits::Breakpoints ||
        pointsCount > uroa::wire::ProgramBundleLimits::Points ||
        uint64_t(pcOffset) + uint64_t(bpCount + pointsCount) * 2 != total_) {
        return Format;
    }
    const Code valid = validate(p + uroa::wire::BundleHeaderLayout::ByteLength, length);
    if (valid != Ok || !validatePcs(p + uroa::wire::BundleHeaderLayout::ByteLength, length, p + pcOffset, bpCount) ||
        !validatePcs(p + uroa::wire::BundleHeaderLayout::ByteLength, length, p + pcOffset + bpCount * 2, pointsCount)) {
        return Invalid;
    }
    if (!current_(session_) || programRevision == UINT32_MAX) {
        return Start;
    }
    startRequested = (header.flags & 1) != 0;
    programLength = length;
    breakpointCount = bpCount;
    stepCount = pointsCount;
    // ESP32はlittle endian。new[]先頭+偶数offsetのPC列はuint16_tとして整列する。
    std::memmove(pending_.get(), p + uroa::wire::BundleHeaderLayout::ByteLength, length);
    program = std::move(pending_);
    breakpoints = reinterpret_cast<const uint16_t *>(program.get() + pcOffset);
    stepPoints = breakpoints + bpCount;
    ++programRevision;
    return Ok;
}

/**
 * @brief PC設定と必要なVM開始の結果を確定する。
 * @param success 成否。
 */
void BytecodeReceiver::completeDeployment(bool success) {
    if (!lock_) {
        return;
    }
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (deploymentPending) {
        deploymentPending = false;
        if (!success || !current_(session_)) {
            reject(Start);
        }
        else {
            state_ = startRequested ? Started : Ready;
            dirty_ = true;
        }
    }
    xSemaphoreGive(lock_);
    publishResult();
}

/**
 * @brief 未完了転送・開始待ちを取り消す。
 */
void BytecodeReceiver::cancelDeployment() {
    if (!lock_) {
        return;
    }
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (receiving_ || sealed_ || deploymentPending) {
        deploymentPending = false;
        reject(Start);
    }
    xSemaphoreGive(lock_);
}

/**
 * @brief 排他して結果をコピーした後、排他外で配送する。
 */
void BytecodeReceiver::publishResult() {
    uroa::wire::TransferResult packet{};
    packet.version = uroa::wire::TransferResultLayout::versionDefault;
    packet.frame = MaxFrame;
    ControlSession session;
    if (lock_) {
        xSemaphoreTake(lock_, portMAX_DELAY);
        packet.code = error_;
        packet.transfer = transferId_;
        packet.op = operation_;
        packet.state = state_;
        packet.received = received_;
        packet.revision = programRevision;
        session = session_;
        dirty_ = false;
        xSemaphoreGive(lock_);
    }
    else {
        packet.code = Memory;
        packet.state = Failed;
    }
    uint8_t result[uroa::wire::TransferResultLayout::ByteLength];
    uroa::wire::writeTransferResult(result, packet);
    if (publish_) {
        publish_(result, sizeof result, session);
    }
}

/**
 * @brief ヘッダー・関数表・命令境界・オペランドを検証する。
 * @param data 全体。
 * @param length 長さ。
 * @return 結果。
 *
 * @details
 * - バイナリの識別子・版・開始PC・予約領域・全体長・上限。
 * - 関数範囲の順序・連続性・長さ、フレームのスロット数。
 * - 共通bytecode_format.cppによるversion=7の固定配置・本体型表・命令長。
 * - opcode・所属範囲、各範囲の末尾HALT／RET。
 * - 大域・局所変数の位置、CALL先の関数入口への一致。
 * - 分岐先が同じ所属範囲内の命令先頭であること。
 * - float32リテラルが有限であること。
 * - 根の数が引数・局所領域内であり、GC_LOOPの入口PCが所属スコープと一致すること。
 */
BytecodeReceiver::Code BytecodeReceiver::validate(const uint8_t *data, size_t length) {
    BytecodeFormat::Image view;
    if (!BytecodeFormat::parseImage(data, length, view)) {
        return Invalid;
    }
    const uint16_t globals = view.globalCount;
    const uint16_t functions = view.functionCount;
    const uint32_t codeLength = view.codeLength;
    const uint16_t strings = view.stringCount;
    const uint32_t dataLength = view.stringDataLength;
    uint32_t expectedOffset = 0;
    for (size_t index = 0; index < strings; ++index) {
        const uint8_t *record = view.strings + index * BytecodeFormat::StringRecordLength;
        const uint32_t offset = read32(record);
        const uint32_t size = read32(record + 4);
        if (offset != expectedOffset || offset > dataLength || size > dataLength - offset ||
            !validUtf8(view.stringData + offset, size)) {
            return Invalid;
        }
        expectedOffset += size;
    }
    if (expectedOffset != dataLength) {
        return Invalid;
    }
    auto function = [&](size_t index) {
        return view.functions + index * BytecodeFormat::FunctionRecordLength;
    };
    uint32_t previousEnd = 0;
    for (size_t index = 0; index < functions; ++index) {
        const uint8_t *record = function(index);
        const uint32_t start = read16(record);
        const uint32_t end = start + read16(record + 2);
        if (start == 0 || end <= start || end > codeLength || (index > 0 && start != previousEnd) ||
            uint32_t(read16(record + 4)) + read16(record + 6) + 2 > 65536 ||
            (read16(record + 8) == 0 || read16(record + 10) < read16(record + 4) ||
                read16(record + 10) > uint32_t(read16(record + 4)) + read16(record + 6))) {
            return Invalid;
        }
        previousEnd = end;
    }
    if (functions > 0 && previousEnd != codeLength) {
        return Invalid;
    }
    std::unique_ptr<uint8_t[]> boundaries(new (std::nothrow) uint8_t[(codeLength + 7) / 8]());
    if (!boundaries) {
        return Memory;
    }
    const uint8_t *code = view.code;
    // 1周目で命令先頭を記録し、2周目で前方への分岐も命令先頭か調べる。
    for (int pass = 0; pass < 2; ++pass) {
        int owner = -1;
        uint32_t pc = 0;
        while (pc < codeLength) {
            if (owner + 1 < functions && pc == read16(function(owner + 1))) {
                ++owner;
            }
            const uint32_t base = owner < 0 ? 0 : read16(function(owner));
            const uint32_t end =
                owner < 0 ? (functions > 0 ? read16(function(0)) : codeLength) : base + read16(function(owner) + 2);
            const uint8_t opcode = code[pc];
            const uint32_t size = BytecodeFormat::instructionLength(opcode);
            if (size == 0 || pc >= end || size > end - pc ||
                (pc + size == end && opcode != (owner < 0 ? Opcode::HALT : Opcode::RET))) {
                return Invalid;
            }
            boundaries[pc / 8] |= uint8_t(1U << (pc % 8));
            const uint16_t value = size == 3 ? read16(code + pc + 1) : 0;
            if (opcode == Opcode::GC_LOOP && value != base) {
                return Invalid;
            }
            if ((opcode == Opcode::PUSH_STRING || opcode == Opcode::FORMAT_VALUE) && value >= strings) {
                return Invalid;
            }
            if (opcode == Opcode::CALL_NATIVE && value == 0) {
                return Invalid;
            }
            if (opcode == Opcode::NEW_OBJECT || opcode == Opcode::CALL_NUMPY) {
                const uint8_t *type = BytecodeFormat::findType(view.types, view.typeCount, value);
                if (!type || (opcode == Opcode::NEW_OBJECT ? type[8] != 0 : type[8] == 0)) {
                    return Invalid;
                }
            }
            if (((opcode == Opcode::GET_ITEM || opcode == Opcode::SET_ITEM) && (value < 1 || value > 4)) ||
                (opcode == Opcode::ARRAY_ATTR && value > 4)) {
                return Invalid;
            }
            if (opcode == Opcode::CHECK_REF && value == 0) {
                return Invalid;
            }
            if ((opcode == Opcode::LOAD_G || opcode == Opcode::STORE_G) && value >= globals) {
                return Invalid;
            }
            if (opcode == Opcode::LOAD_L || opcode == Opcode::STORE_L) {
                if (owner < 0 || value < 2 ||
                    value >= uint32_t(read16(function(owner) + 4)) + read16(function(owner) + 6) + 2) {
                    return Invalid;
                }
            }
            if (opcode >= Opcode::JUMP && opcode <= Opcode::JUMP_IF_TRUE) {
                const uint32_t target = base + value;
                if (target >= end || (pass == 1 && (boundaries[target / 8] & (1U << (target % 8))) == 0)) {
                    return Invalid;
                }
            }
            if (opcode == Opcode::CALL) {
                size_t low = 0;
                size_t high = functions;
                while (low < high) {
                    const size_t middle = (low + high) / 2;
                    if (read16(function(middle)) < value) {
                        low = middle + 1;
                    }
                    else {
                        high = middle;
                    }
                }
                if (low == functions || read16(function(low)) != value) {
                    return Invalid;
                }
            }
            if ((opcode == Opcode::RET && owner < 0) || (opcode == Opcode::HALT && owner >= 0) ||
                (opcode == Opcode::PUSH_FLOAT && (read32(code + pc + 1) & 0x7F800000) == 0x7F800000)) {
                return Invalid;
            }
            pc += size;
        }
    }
    return Ok;
}
