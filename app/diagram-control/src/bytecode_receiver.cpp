/**
 * @file bytecode_receiver.cpp
 * @brief 分割受信・CRC・バイナリの構造を検証し、正常なプログラムだけをRAMへ確定する。
 * @details
 * 転送version=1の20バイトフレームで、バイナリversion=1・2・3を受信する。
 * 形式の共通仕様はuroa/diagram/ts/python/bytecode.ts先頭、送信側の仕様は
 * uroa/diagram/ts/ble-wifi/bytecode-transfer.ts先頭を参照する。
 * version=1は16バイトヘッダー・関数表・命令列、version=2は24バイトヘッダー・
 * 関数表・命令列・文字列表・UTF-8データ。文字列表のレコードはoffset:uint32と
 * length:uint32で、データ先頭からの位置とバイト長を持つ。全数値はlittle endian。
 * 全体は128KiB以内、命令列は1〜65536バイト。version=2の予約位置18も0を必須とする。
 * version=3は56バイトヘッダー、12バイト関数表、命令列、文字列表、UTF-8データの順。
 * ヘッダー位置14の呼出し規約版と位置18のAPI形式版はともに2。位置24の32バイトは
 * ABI指紋としてイメージ内に保持する。関数表は従来の8バイトに戻り値スロット数:uint16
 * （1以上）と予約:uint16（0）を追加する。引数・局所・戻り値は構造体を展開したスロット数。
 * 文字列はID順の連続配置を検証し、過長表現・サロゲート・範囲外を含む不正UTF-8を拒否する。
 * 空文字列と埋め込みNULは許可し、DATAの途中では文字列を解釈しない。
 * PUSH_STRING(0x19)はversion=2・3で許可する3バイト命令で、IDが文字列数未満か検査する。
 * version=3だけの追加命令はCALL_NATIVE(0x1A、uint16の非0 ID)、TO_FLOAT(0x1B、1バイト)、
 * FORMAT_VALUE(0x1C、文字列数未満のuint16 ID)、BUILD_STRING(0x1D、uint16の断片数)。
 * 追加命令のうちオペランドを持つものは3バイト。BUILD_STRINGの断片数0は許可する。これらの命令を
 * version=1・2で使用した場合は形式不正にする。
 * 文字列表・データは命令走査と分岐先の範囲に含めない。既存命令と関数フレームの検証は維持する。
 * CRC・順序・接続世代・期限・確定前の全件検証に成功した場合だけRAMの確定バッファを置換する。
 * readyは保持完了だけを表す。main.cppは自動開始せず、別途VM_START指令でVM開始を試みる。
 * この受信段階ではABI指紋の一致とnativeIdの存在・引数型は照合しない。
 * 登録スタブ表との照合・呼出し・実行時の値と寿命の検査はbytecode_vm.cppが担当する。
 * readyはスタブ互換性や実行成功を表さない。機器のスタブはdevice_api.generated.cppで接続する。
 */
#include "bytecode_receiver.h"
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

    /** @brief 非整列のlittle endian値を読む。@param p 2バイトの先頭。@return uint16値。 */
    uint16_t read16(const uint8_t *p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }

    /** @brief 非整列のlittle endian値を読む。@param p 4バイトの先頭。@return uint32値。 */
    uint32_t read32(const uint8_t *p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }

    /** @brief uint16をlittle endianで書く。@param p 出力先。@param value 数値。 */
    void write16(uint8_t *p, uint16_t value) {
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
    }

    /** @brief uint32をlittle endianで書く。@param p 出力先。@param value 数値。 */
    void write32(uint8_t *p, uint32_t value) {
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
        p[2] = uint8_t(value >> 16);
        p[3] = uint8_t(value >> 24);
    }
}

/**
 * @brief 受信キューとコールバックを準備する。
 * @param publish setup()・loop()から呼ぶ結果の出力先。
 * @param current loop()で確定直前に接続世代を再照合する関数。
 */
void BytecodeReceiver::begin(const std::function<void(const uint8_t *, size_t)> &publish,
    const std::function<bool(uint32_t)> &current) {
    publish_ = publish;
    current_ = current;
    frames_ = xQueueCreate(4, sizeof(Frame));
    result_[0] = 1;
    reply(frames_ == nullptr ? Memory : Ok);
}

/** @brief BLEタスクでフレームをコピーする。@param data 入力。@param length 長さ。@param session 接続世代。
 *
 * @details
 * ## ESP32の受信と保持
 *
 * [BleTransport](../../../../esp32/lib/ble/include/uroa_ble.h)は20バイト以内のフレームを`Callbacks::bytecode`へ渡す。[BytecodeReceiver](../../../../esp32/app/diagram-control/include/bytecode_receiver.h)の`enqueue()`は接続世代とともに4件の専用FreeRTOSキューへコピーする。BLEコールバックで動的確保・CRC・バイナリの解析を行わない。
 */
void BytecodeReceiver::enqueue(const uint8_t *data, size_t length, uint32_t session) {
    Frame frame;
    frame.session = session;
    if (length <= sizeof frame.data) {
        frame.length = uint8_t(length);
        if (length > 0) {
            std::memcpy(frame.data, data, length);
        }
    }
    if (frames_ == nullptr || xQueueSend(frames_, &frame, 0) != pdTRUE) {
        overflow_.store(true);
    }
}

/** @brief 未完了の受信だけを破棄し、確定済みプログラムを残す。 */
void BytecodeReceiver::cancel() {
    pending_.reset();
    receiving_ = false;
}

/** @brief 最新の受付結果を再公開する。 */
void BytecodeReceiver::publishResult() {
    if (publish_) {
        publish_(result_, sizeof result_);
    }
}

/** @brief 結果20バイトを更新する。@param code 結果。@param frame 対象フレーム。nullなら直前の番号を維持する。 */
void BytecodeReceiver::reply(Code code, const Frame *frame) {
    result_[0] = 1;
    result_[1] = code;
    if (frame != nullptr) {
        write16(result_ + 2, frame->length >= 4 ? read16(frame->data + 2) : 0);
        write16(result_ + 4, frame->length >= 6 ? read16(frame->data + 4) : 0);
        result_[6] = frame->length >= 2 ? frame->data[1] : 0;
    }
    result_[7] = code != Ok ? 3 : receiving_ ? 1 : program ? 2 : 0;
    write32(result_ + 8, received_);
    write32(result_ + 12, total_);
    write32(result_ + 16, expectedCrc_);
    publishResult();
}

/** @brief 現在のBLE接続世代のフレームと受信期限を処理する。@param connected BLE制御中か。@param session 世代。
 *
 * @details
 * 切断・BLE停止・Wi-Fi切替・接続世代の変更、受信キュー飽和、受信待ち15秒超過、ABORT、不正フレームでは未完了の受信を破棄する。キュー飽和やタイミングによっては対象sequenceの結果が返らず、Web側の2.5秒タイムアウトから再接続が必要になる。
 */
void BytecodeReceiver::process(bool connected, uint32_t session) {
    if (connected != connected_ || session != session_) {
        connected_ = connected;
        session_ = session;
        cancel();
        if (frames_ != nullptr) {
            xQueueReset(frames_);
        }
        std::memset(result_, 0, sizeof result_);
        received_ = 0;
        total_ = 0;
        expectedCrc_ = 0;
        reply(frames_ == nullptr ? Memory : Ok);
    }
    if (overflow_.exchange(false)) {
        cancel();
        if (frames_ != nullptr) {
            xQueueReset(frames_);
        }
        reply(Busy);
    }
    if (receiving_ && uint32_t(millis() - lastReceive_) > 15000) {
        cancel();
        reply(Timeout);
    }
    if (frames_ == nullptr) {
        return;
    }
    Frame frame;
    for (int count = 0; count < 4 && xQueueReceive(frames_, &frame, 0) == pdTRUE; ++count) {
        if (connected_ && frame.session == session_) {
            handle(frame);
        }
    }
}

/** @brief 現接続のフレームを解釈する。@param frame キューから取り出したコピー。
 *
 * @details
 * `loop()`の`process()`は現BLE接続世代のデータだけを処理する。BEGINで受信バッファを確保し、DATAのID・sequence・連続位置・全体長を検査してコピーする。COMMIT時に受信長・CRCと次の構造を全件検査する。
 *
 * 全件に成功し、確定直前にも現在のBLE接続・世代を再照合できたときだけ、公開メンバー`program`・`programLength`・`programCrc32`を置き換え、readyを返す。公開メンバーはloop()だけが更新し、VMは確定バッファを借用し、loop()が実行前に確定世代を照合する。受信中・検証失敗時は以前の確定済みプログラムを保持するため、新旧2個分のRAMが必要になる場合がある。確保に失敗したら以前のプログラムを残して拒否する。
 *
 * 確定済みデータは切断後もRAMに残るが再起動で失われる。受信完了時はシリアルに`Bytecode received: bytes=... crc32=...`を出す。
 */
void BytecodeReceiver::handle(const Frame &frame) {
    const uint8_t *p = frame.data;
    auto reject = [&](Code code) {
        cancel();
        reply(code, &frame);
    };
    if (frame.length < 10 || p[0] != 1 || read16(p + 2) == 0 || read16(p + 4) == 0) {
        reject(Format);
        return;
    }
    const uint8_t operation = p[1];
    const uint16_t id = read16(p + 2);
    const uint16_t sequence = read16(p + 4);
    const uint32_t position = read32(p + 6);
    if (operation == 1) {
        if (frame.length != 14 || sequence != 1) {
            reject(Format);
            return;
        }
        if (position < 16 || position > MaxLength) {
            reject(Range);
            return;
        }
        cancel();
        transferId_ = id;
        sequence_ = sequence;
        total_ = position;
        received_ = 0;
        expectedCrc_ = read32(p + 10);
        crc_ = 0xFFFFFFFF;
        pending_.reset(new (std::nothrow) uint8_t[total_]);
        if (!pending_) {
            reject(Memory);
            return;
        }
        receiving_ = true;
        lastReceive_ = millis();
        reply(Ok, &frame);
        return;
    }
    if (id != transferId_ || sequence != uint16_t(sequence_ + 1) || (!receiving_ && operation != 4)) {
        reject(Order);
        return;
    }
    sequence_ = sequence;
    if (operation == 4 && frame.length == 10 && position == 0) {
        cancel();
        received_ = 0;
        total_ = 0;
        expectedCrc_ = 0;
        reply(Ok, &frame);
        return;
    }
    if (operation == 2 && frame.length > 10) {
        const size_t length = frame.length - 10;
        if (position != received_ || length > total_ - received_) {
            reject(Order);
            return;
        }
        std::memcpy(pending_.get() + received_, p + 10, length);
        for (size_t index = 10; index < frame.length; ++index) {
            crc_ ^= p[index];
            for (int bit = 0; bit < 8; ++bit) {
                crc_ = (crc_ >> 1) ^ ((crc_ & 1) != 0 ? 0xEDB88320 : 0);
            }
        }
        received_ += length;
        lastReceive_ = millis();
        reply(Ok, &frame);
        return;
    }
    if (operation != 3 || frame.length != 10) {
        reject(Format);
        return;
    }
    if (position != total_ || received_ != total_) {
        reject(Order);
        return;
    }
    if ((crc_ ^ 0xFFFFFFFF) != expectedCrc_) {
        reject(Crc);
        return;
    }
    const Code valid = validate(pending_.get(), total_);
    if (valid != Ok) {
        reject(valid);
        return;
    }
    if (!current_ || !current_(session_)) {
        cancel();
        return;
    }
    program = std::move(pending_);
    programLength = total_;
    programCrc32 = expectedCrc_;
    ++programRevision;
    receiving_ = false;
    reply(Ok, &frame);
    Serial.printf("Bytecode received: bytes=%u crc32=%08lx\n", unsigned(programLength),
        static_cast<unsigned long>(programCrc32));
}

/** @brief ヘッダー・関数表・命令境界・オペランドを検証する。@param data 全体。@param length 長さ。@return 結果。
 *
 * @details
 * - バイナリの識別子・版・開始PC・予約領域・全体長・上限。
 * - 関数範囲の順序・連続性・長さ、フレームのスロット数。
 * - opcode・命令長・所属範囲、各範囲の末尾HALT／RET。
 * - 大域・局所変数の位置、CALL先の関数入口への一致。
 * - 分岐先が同じ所属範囲内の命令先頭であること。
 * - float32リテラルが有限であること。
 */
BytecodeReceiver::Code BytecodeReceiver::validate(const uint8_t *data, size_t length) {
    if (length < 16 || length > MaxLength || read16(data) != 0xB1C0 || read16(data + 12) != 0) {
        return Invalid;
    }
    const uint16_t version = read16(data + 2);
    if (version != 1 && version != 2 && version != 3) {
        return Invalid;
    }
    const size_t headerLength = version == 1 ? 16 : version == 2 ? 24 : 56;
    const size_t functionRecordLength = version == 3 ? 12 : 8;
    if (length < headerLength || read16(data + 14) != (version == 3 ? 2 : 0) ||
        (version != 1 && read16(data + 18) != (version == 3 ? 2 : 0))) {
        return Invalid;
    }
    const uint16_t globals = read16(data + 4);
    const uint16_t functions = read16(data + 6);
    const uint32_t codeLength = read32(data + 8);
    const uint16_t strings = version == 1 ? 0 : read16(data + 16);
    const uint32_t dataLength = version == 1 ? 0 : read32(data + 20);
    const size_t codeOffset = headerLength + size_t(functions) * functionRecordLength;
    if (codeOffset > length || codeLength < 1 || codeLength > 65536 || codeLength > length - codeOffset) {
        return Invalid;
    }
    const size_t stringTableOffset = codeOffset + codeLength;
    if (size_t(strings) > (length - stringTableOffset) / 8) {
        return Invalid;
    }
    const size_t stringDataOffset = stringTableOffset + size_t(strings) * 8;
    if (dataLength != length - stringDataOffset) {
        return Invalid;
    }
    uint32_t expectedOffset = 0;
    for (size_t index = 0; index < strings; ++index) {
        const uint8_t *record = data + stringTableOffset + index * 8;
        const uint32_t offset = read32(record);
        const uint32_t size = read32(record + 4);
        if (offset != expectedOffset || offset > dataLength || size > dataLength - offset ||
            !validUtf8(data + stringDataOffset + offset, size)) {
            return Invalid;
        }
        expectedOffset += size;
    }
    if (expectedOffset != dataLength) {
        return Invalid;
    }
    auto function = [&](size_t index) {
        return data + headerLength + index * functionRecordLength;
    };
    uint32_t previousEnd = 0;
    for (size_t index = 0; index < functions; ++index) {
        const uint8_t *record = function(index);
        const uint32_t start = read16(record);
        const uint32_t end = start + read16(record + 2);
        if (start == 0 || end <= start || end > codeLength || (index > 0 && start != previousEnd) ||
            uint32_t(read16(record + 4)) + read16(record + 6) + 2 > 65536 ||
            (version == 3 && (read16(record + 8) == 0 || read16(record + 10) != 0))) {
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
    const uint8_t *code = data + codeOffset;
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
            if (opcode < 0x01 || opcode > (version == 1 ? 0x18 : version == 2 ? 0x19 : 0x1D)) {
                return Invalid;
            }
            const bool operand = opcode <= 0x09 || opcode == 0x14 || opcode == 0x19 ||
                opcode == 0x1A || opcode == 0x1C || opcode == 0x1D;
            const uint32_t size = opcode == 0x03 || opcode == 0x04 ? 5 : operand ? 3 : 1;
            if (pc + size > end || (pc + size == end && opcode != (owner < 0 ? 0x18 : 0x15))) {
                return Invalid;
            }
            boundaries[pc / 8] |= uint8_t(1U << (pc % 8));
            const uint16_t value = operand ? read16(code + pc + 1) : 0;
            if ((opcode == 0x19 || opcode == 0x1C) && value >= strings) {
                return Invalid;
            }
            if (opcode == 0x1A && value == 0) {
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
