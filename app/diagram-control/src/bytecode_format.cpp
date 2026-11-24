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
    if (!data || length < HeaderLength || length > MaxLength || read16(data) != 0xB1C0 ||
        read16(data + 2) != Version || read16(data + 12) != 0 || read16(data + 14) != 2 || read16(data + 18) != 2) {
        return false;
    }
    Image result;
    result.globalCount = read16(data + 4);
    result.functionCount = read16(data + 6);
    result.stringCount = read16(data + 16);
    result.codeLength = read32(data + 8);
    result.stringDataLength = read32(data + 20);
    const size_t codeOffset = HeaderLength + size_t(result.functionCount) * FunctionRecordLength;
    const uint64_t expectedLength = uint64_t(codeOffset) + result.codeLength +
        uint64_t(result.stringCount) * StringRecordLength + result.stringDataLength;
    if (result.codeLength == 0 || result.codeLength > 65536 || expectedLength != length) {
        return false;
    }
    result.functions = data + HeaderLength;
    result.code = data + codeOffset;
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
    if (opcode < 0x01 || opcode > 0x1D) {
        return 0;
    }
    if (opcode == 0x03 || opcode == 0x04) {
        return 5;
    }
    if (opcode <= 0x09 || opcode == 0x14 || opcode == 0x19 || opcode == 0x1A || opcode == 0x1C || opcode == 0x1D) {
        return 3;
    }
    return 1;
}
