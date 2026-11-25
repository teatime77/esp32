/**
 * @file bytecode_format.cpp
 * @brief 受信器・VM・デバッガーで共有する、バイトコード配置と命令長の検証。
 */
#include "bytecode_format.h"

namespace {
    /**
     * @brief 非整列のlittle endian整数を読む。
     * @param data 読取り可能な2バイト。
     * @return 復号値。
     */
    uint16_t read16(const uint8_t *data) {
        return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    }

    /**
     * @brief 非整列のlittle endian整数を読む。
     * @param data 読取り可能な4バイト。
     * @return 復号値。
     */
    uint32_t read32(const uint8_t *data) {
        return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
    }
}

/**
 * @copydoc BytecodeFormat::parseImage
 */
bool BytecodeFormat::parseImage(const uint8_t *data, size_t length, Image &image) {
    if (!data || length < HeaderLength || length > MaxLength || read16(data) != 0xB1C0 || read16(data + 2) != Version ||
        read16(data + 12) != 0 || read16(data + 14) != 3 || read16(data + 18) != 3 || read16(data + 58) != 0) {
        return false;
    }
    Image result;
    result.globalCount = read16(data + 4);
    result.functionCount = read16(data + 6);
    result.stringCount = read16(data + 16);
    result.codeLength = read32(data + 8);
    result.stringDataLength = read32(data + 20);
    result.typeCount = read16(data + 56);
    result.typeLength = read32(data + 60);
    const size_t typeOffset = HeaderLength + size_t(result.functionCount) * FunctionRecordLength;
    const uint64_t codeOffset = uint64_t(typeOffset) + result.typeLength;
    const uint64_t expectedLength = uint64_t(codeOffset) + result.codeLength +
        uint64_t(result.stringCount) * StringRecordLength + result.stringDataLength;
    if (result.codeLength == 0 || result.codeLength > 65536 || expectedLength != length) {
        return false;
    }
    result.functions = data + HeaderLength;
    result.types = data + typeOffset;
    if (result.typeLength < size_t(result.typeCount) * TypeRecordLength) {
        return false;
    }
    uint32_t fieldOffset = uint32_t(result.typeCount) * TypeRecordLength;
    uint16_t previousId = 0;
    for (size_t index = 0; index < result.typeCount; ++index) {
        const uint8_t *record = result.types + index * TypeRecordLength;
        const uint16_t id = read16(record);
        const uint32_t bytes = uint32_t(read16(record + 2)) * 4;
        if (id <= previousId || read32(record + 4) != fieldOffset || bytes > result.typeLength - fieldOffset) {
            return false;
        }
        previousId = id;
        fieldOffset += bytes;
    }
    if (fieldOffset != result.typeLength) {
        return false;
    }
    for (uint32_t offset = uint32_t(result.typeCount) * TypeRecordLength; offset < result.typeLength; offset += 4) {
        const uint8_t kind = result.types[offset];
        const uint16_t id = read16(result.types + offset + 2);
        if ((kind != 1 && kind != 2 && kind != 3 && kind != 6 && kind != 7 && kind != 8) ||
            (result.types[offset + 1] & ~3U) != 0 || ((kind == 7 || kind == 8) ? id == 0 : id != 0) ||
            (kind == 8 && !findType(result.types, result.typeCount, id))) {
            return false;
        }
    }
    result.code = data + size_t(codeOffset);
    result.strings = result.code + result.codeLength;
    result.stringData = result.strings + size_t(result.stringCount) * StringRecordLength;
    result.abiHash = data + 24;
    image = result;
    return true;
}

/**
 * @copydoc BytecodeFormat::instructionLength
 */
uint32_t BytecodeFormat::instructionLength(uint8_t opcode) {
    if (opcode < 0x01 || opcode > 0x21) {
        return 0;
    }
    if (opcode == 0x03 || opcode == 0x04) {
        return 5;
    }
    if (opcode <= 0x09 || opcode == 0x14 || opcode == 0x19 || opcode == 0x1A || opcode == 0x1C || opcode == 0x1D ||
        (opcode >= 0x1E && opcode <= 0x20)) {
        return 3;
    }
    return 1;
}

/**
 * @copydoc BytecodeFormat::findType
 */
const uint8_t *BytecodeFormat::findType(const uint8_t *types, uint16_t count, uint16_t id) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        const size_t middle = (low + high) / 2;
        if (read16(types + middle * TypeRecordLength) < id) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    return low < count && read16(types + low * TypeRecordLength) == id ? types + low * TypeRecordLength : nullptr;
}
