/**
 * @file bytecode_receiver.cpp
 * @brief 一括転送version=2を蓄積しloop()でコード・PCを検証する。
 * @details 通信の中心契約はuroa/diagram/ts/ble-wifi/bytecode-transfer.ts先頭。
 * 受信領域の確保・コピーは通信タスクでmutex排他し、CRC・命令検証・確定はloop()で行う。
 * 確定コードはRAMへ保持し、Flash保存は行わない。
 */
#include "bytecode_receiver.h"
#include "bytecode_format.h"
#include <cstring>
#include <new>

namespace {
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
            if (codePoint < minimum || codePoint > 0x10FFFF ||
                (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
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

    /**
     * @brief uint32をlittle endianで書く。
     * @param p 出力先。
     * @param value 数値。
     */
    void write32(uint8_t *p, uint32_t value) {
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
        p[2] = uint8_t(value >> 16);
        p[3] = uint8_t(value >> 24);
    }
    /**
     * @brief CRC-32/ISO-HDLC。
     * @param data 入力。
     * @param length 長さ。
     * @return CRC。
     */
    uint32_t crc32(const uint8_t *data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
        }
        return crc ^ 0xFFFFFFFF;
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
    state_ = 3;
    dirty_ = true;
}

/**
 * @brief 通信タスクでヘッダーを検査して専用領域へコピーする。
 * @param data フレーム。
 * @param length 長さ。
 * @param session 接続世代。
 * @details BEGINで最大MaxBundleまで確保する。DATAは順序・範囲だけを検査し、CRCやVMを処理しない。
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
        state_ = 0;
    }
    if (length < 10 || length > MaxFrame || data[0] != 2) {
        reject(Format);
        xSemaphoreGive(lock_);
        return;
    }
    const uint8_t op = data[1];
    const uint16_t id = read16(data + 2);
    const uint16_t seq = read16(data + 4);
    const uint32_t pos = read32(data + 6);
    if (op == 1 && id > transferId_ && seq == 1 && !deploymentPending) {
        pending_.reset();
        transferId_ = id;
        sequence_ = 1;
        operation_ = 1;
        error_ = Ok;
        received_ = 0;
        total_ = pos;
        state_ = 1;
        sealed_ = false;
        receiving_ = false;
        if (length != 14 || pos < 40 || pos > MaxBundle) {
            reject(Range);
        }
        else {
            expectedCrc_ = read32(data + 10);
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
    if (id != transferId_ || error_ != Ok || (!receiving_ && op == 3) || sealed_) {
        xSemaphoreGive(lock_);
        return;
    }
    if (!receiving_ || seq != uint16_t(sequence_ + 1)) {
        reject(Order);
    }
    else {
        sequence_ = seq;
        operation_ = op;
        if (op == 2 && length > 10 && pos == received_ && length - 10 <= total_ - received_) {
            std::memcpy(pending_.get() + received_, data + 10, length - 10);
            received_ += length - 10;
            lastReceive_ = millis();
        }
        else if (op == 3 && length == 10 && pos == total_ && received_ == total_) {
            sealed_ = true;
            receiving_ = false;
        }
        else if (op == 4 && length == 10 && pos == 0) {
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
        state_ = 0;
        operation_ = 0;
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
    if (!p || total_ < 40 || crc32(p, total_) != expectedCrc_) {
        return Crc;
    }
    const uint32_t length = read32(p + 8);
    const uint32_t bpCount = read32(p + 12);
    const uint32_t pointsCount = read32(p + 16);
    const size_t pcOffset = 24 + size_t(length) + length % 2;
    if (read32(p) != 0x32525055 || read16(p + 4) != 1 || read16(p + 6) > 1 || length < 16 || length > MaxLength ||
        bpCount > 4096 || pointsCount > 65536 || uint64_t(pcOffset) + uint64_t(bpCount + pointsCount) * 2 != total_) {
        return Format;
    }
    if ((length % 2 && p[24 + length] != 0) || crc32(p + 24, length) != read32(p + 20)) {
        return Crc;
    }
    const Code valid = validate(p + 24, length);
    if (valid != Ok || !validatePcs(p + 24, length, p + pcOffset, bpCount) ||
        !validatePcs(p + 24, length, p + pcOffset + bpCount * 2, pointsCount)) {
        return Invalid;
    }
    if (!current_(session_) || programRevision == UINT32_MAX) {
        return Start;
    }
    startRequested = (read16(p + 6) & 1) != 0;
    programCrc32 = read32(p + 20);
    programLength = length;
    breakpointCount = bpCount;
    stepCount = pointsCount;
    // ESP32はlittle endian。new[]先頭+偶数offsetのPC列はuint16_tとして整列する。
    std::memmove(pending_.get(), p + 24, length);
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
            state_ = startRequested ? 4 : 2;
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
    uint8_t result[20]{};
    result[0] = 2;
    ControlSession session;
    if (lock_) {
        xSemaphoreTake(lock_, portMAX_DELAY);
        result[1] = error_;
        result[2] = uint8_t(transferId_);
        result[3] = uint8_t(transferId_ >> 8);
        result[6] = operation_;
        result[7] = state_;
        write32(result + 8, received_);
        write32(result + 12, programRevision);
        write32(result + 16, programCrc32);
        session = session_;
        dirty_ = false;
        xSemaphoreGive(lock_);
    }
    else {
        result[1] = Memory;
        result[7] = 3;
    }
    result[4] = uint8_t(MaxFrame);
    result[5] = uint8_t(MaxFrame >> 8);
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
 * - 共通bytecode_format.cppによるversion=6の固定配置・本体型表・命令長。
 * - opcode・所属範囲、各範囲の末尾HALT／RET。
 * - 大域・局所変数の位置、CALL先の関数入口への一致。
 * - 分岐先が同じ所属範囲内の命令先頭であること。
 * - float32リテラルが有限であること。
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
            (read16(record + 8) == 0 || read16(record + 10) != 0)) {
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
            const uint32_t end = owner < 0 ? (functions > 0 ? read16(function(0)) : codeLength)
                                         : base + read16(function(owner) + 2);
            const uint8_t opcode = code[pc];
            const uint32_t size = BytecodeFormat::instructionLength(opcode);
            if (size == 0 || pc >= end || size > end - pc || (pc + size == end && opcode != (owner < 0 ? 0x18 : 0x15))) {
                return Invalid;
            }
            boundaries[pc / 8] |= uint8_t(1U << (pc % 8));
            const uint16_t value = size == 3 ? read16(code + pc + 1) : 0;
            if ((opcode == 0x19 || opcode == 0x1C) && value >= strings) {
                return Invalid;
            }
            if (opcode == 0x1A && value == 0) {
                return Invalid;
            }
            if (opcode == 0x1E || opcode == 0x22) {
                const uint8_t *type = BytecodeFormat::findType(view.types, view.typeCount, value);
                if (!type || (opcode == 0x1E ? type[8] != 0 : type[8] == 0)) {
                    return Invalid;
                }
            }
            if (((opcode == 0x23 || opcode == 0x24) && (value < 1 || value > 4)) || (opcode == 0x25 && value > 4)) {
                return Invalid;
            }
            if (opcode == 0x26 && value == 0) {
                return Invalid;
            }
            if ((opcode == 0x02 || opcode == 0x06) && value >= globals) {
                return Invalid;
            }
            if (opcode == 0x01 || opcode == 0x05) {
                if (owner < 0 || value < 2 ||
                    value >= uint32_t(read16(function(owner) + 4)) + read16(function(owner) + 6) + 2) {
                    return Invalid;
                }
            }
            if (opcode >= 0x07 && opcode <= 0x09) {
                const uint32_t target = base + value;
                if (target >= end || (pass == 1 && (boundaries[target / 8] & (1U << (target % 8))) == 0)) {
                    return Invalid;
                }
            }
            if (opcode == 0x14) {
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
            if ((opcode == 0x15 && owner < 0) || (opcode == 0x18 && owner >= 0) ||
                (opcode == 0x04 && (read32(code + pc + 1) & 0x7F800000) == 0x7F800000)) {
                return Invalid;
            }
            pc += size;
        }
    }
    return Ok;
}
