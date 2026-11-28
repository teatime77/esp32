/**
 * @file uroa_wire.generated.cpp
 * @brief .protoの宣言順・little endianの部分領域codec。手動編集しない。
 */
#include "uroa_wire.generated.h"
#include <string.h>
namespace uroa {
    namespace wire {
        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        TransferHeader readTransferHeader(const uint8_t *data) {
            TransferHeader packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t transferValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.transfer = static_cast<uint16_t>(transferValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t positionValue = (static_cast<uint32_t>(data[6])) | (static_cast<uint32_t>(data[7]) << 8) | (static_cast<uint32_t>(data[8]) << 16) | (static_cast<uint32_t>(data[9]) << 24);
            packet.position = static_cast<uint32_t>(positionValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        TransferBegin readTransferBegin(const uint8_t *data) {
            TransferBegin packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t transferValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.transfer = static_cast<uint16_t>(transferValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t positionValue = (static_cast<uint32_t>(data[6])) | (static_cast<uint32_t>(data[7]) << 8) | (static_cast<uint32_t>(data[8]) << 16) | (static_cast<uint32_t>(data[9]) << 24);
            packet.position = static_cast<uint32_t>(positionValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[10])) | (static_cast<uint32_t>(data[11]) << 8) | (static_cast<uint32_t>(data[12]) << 16) | (static_cast<uint32_t>(data[13]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        TransferResult readTransferResult(const uint8_t *data) {
            TransferResult packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t codeValue = (static_cast<uint32_t>(data[1]));
            packet.code = static_cast<uint8_t>(codeValue);
            const uint32_t transferValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.transfer = static_cast<uint16_t>(transferValue);
            const uint32_t frameValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8);
            packet.frame = static_cast<uint16_t>(frameValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[6]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t stateValue = (static_cast<uint32_t>(data[7]));
            packet.state = static_cast<uint8_t>(stateValue);
            const uint32_t receivedValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.received = static_cast<uint32_t>(receivedValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[12])) | (static_cast<uint32_t>(data[13]) << 8) | (static_cast<uint32_t>(data[14]) << 16) | (static_cast<uint32_t>(data[15]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[16])) | (static_cast<uint32_t>(data[17]) << 8) | (static_cast<uint32_t>(data[18]) << 16) | (static_cast<uint32_t>(data[19]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            return packet;
        }
        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeTransferResult(uint8_t *out, const TransferResult &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t codeValue = static_cast<uint32_t>(packet.code);
            out[1] = static_cast<uint8_t>(codeValue);
            const uint32_t transferValue = static_cast<uint32_t>(packet.transfer);
            out[2] = static_cast<uint8_t>(transferValue);
            out[3] = static_cast<uint8_t>(transferValue >> 8);
            const uint32_t frameValue = static_cast<uint32_t>(packet.frame);
            out[4] = static_cast<uint8_t>(frameValue);
            out[5] = static_cast<uint8_t>(frameValue >> 8);
            const uint32_t opValue = static_cast<uint32_t>(packet.op);
            out[6] = static_cast<uint8_t>(opValue);
            const uint32_t stateValue = static_cast<uint32_t>(packet.state);
            out[7] = static_cast<uint8_t>(stateValue);
            const uint32_t receivedValue = static_cast<uint32_t>(packet.received);
            out[8] = static_cast<uint8_t>(receivedValue);
            out[9] = static_cast<uint8_t>(receivedValue >> 8);
            out[10] = static_cast<uint8_t>(receivedValue >> 16);
            out[11] = static_cast<uint8_t>(receivedValue >> 24);
            const uint32_t revisionValue = static_cast<uint32_t>(packet.revision);
            out[12] = static_cast<uint8_t>(revisionValue);
            out[13] = static_cast<uint8_t>(revisionValue >> 8);
            out[14] = static_cast<uint8_t>(revisionValue >> 16);
            out[15] = static_cast<uint8_t>(revisionValue >> 24);
            const uint32_t crcValue = static_cast<uint32_t>(packet.crc);
            out[16] = static_cast<uint8_t>(crcValue);
            out[17] = static_cast<uint8_t>(crcValue >> 8);
            out[18] = static_cast<uint8_t>(crcValue >> 16);
            out[19] = static_cast<uint8_t>(crcValue >> 24);
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        BundleHeader readBundleHeader(const uint8_t *data) {
            BundleHeader packet{};
            const uint32_t signatureValue = (static_cast<uint32_t>(data[0])) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
            packet.signature = static_cast<uint32_t>(signatureValue);
            const uint32_t versionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8);
            packet.version = static_cast<uint16_t>(versionValue);
            const uint32_t flagsValue = (static_cast<uint32_t>(data[6])) | (static_cast<uint32_t>(data[7]) << 8);
            packet.flags = static_cast<uint16_t>(flagsValue);
            const uint32_t lengthValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.length = static_cast<uint32_t>(lengthValue);
            const uint32_t breakpointsValue = (static_cast<uint32_t>(data[12])) | (static_cast<uint32_t>(data[13]) << 8) | (static_cast<uint32_t>(data[14]) << 16) | (static_cast<uint32_t>(data[15]) << 24);
            packet.breakpoints = static_cast<uint32_t>(breakpointsValue);
            const uint32_t pointsValue = (static_cast<uint32_t>(data[16])) | (static_cast<uint32_t>(data[17]) << 8) | (static_cast<uint32_t>(data[18]) << 16) | (static_cast<uint32_t>(data[19]) << 24);
            packet.points = static_cast<uint32_t>(pointsValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[20])) | (static_cast<uint32_t>(data[21]) << 8) | (static_cast<uint32_t>(data[22]) << 16) | (static_cast<uint32_t>(data[23]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugHeader readDebugHeader(const uint8_t *data) {
            DebugHeader packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.request = static_cast<uint16_t>(requestValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugBegin readDebugBegin(const uint8_t *data) {
            DebugBegin packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.request = static_cast<uint16_t>(requestValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t countValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.count = static_cast<uint32_t>(countValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[12])) | (static_cast<uint32_t>(data[13]) << 8) | (static_cast<uint32_t>(data[14]) << 16) | (static_cast<uint32_t>(data[15]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            const uint32_t imageCrcValue = (static_cast<uint32_t>(data[16])) | (static_cast<uint32_t>(data[17]) << 8) | (static_cast<uint32_t>(data[18]) << 16) | (static_cast<uint32_t>(data[19]) << 24);
            packet.imageCrc = static_cast<uint32_t>(imageCrcValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugData readDebugData(const uint8_t *data) {
            DebugData packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.request = static_cast<uint16_t>(requestValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t indexValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.index = static_cast<uint32_t>(indexValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugChunk readDebugChunk(const uint8_t *data) {
            DebugChunk packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.request = static_cast<uint16_t>(requestValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t stopIdValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.stopId = static_cast<uint32_t>(stopIdValue);
            const uint32_t offsetValue = (static_cast<uint32_t>(data[12])) | (static_cast<uint32_t>(data[13]) << 8) | (static_cast<uint32_t>(data[14]) << 16) | (static_cast<uint32_t>(data[15]) << 24);
            packet.offset = static_cast<uint32_t>(offsetValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugExecute readDebugExecute(const uint8_t *data) {
            DebugExecute packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.request = static_cast<uint16_t>(requestValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t stopIdValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.stopId = static_cast<uint32_t>(stopIdValue);
            return packet;
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeDebugResult(uint8_t *out, const DebugResult &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = static_cast<uint32_t>(packet.op);
            out[1] = static_cast<uint8_t>(opValue);
            const uint32_t requestValue = static_cast<uint32_t>(packet.request);
            out[2] = static_cast<uint8_t>(requestValue);
            out[3] = static_cast<uint8_t>(requestValue >> 8);
            const uint32_t codeValue = static_cast<uint32_t>(packet.code);
            out[4] = static_cast<uint8_t>(codeValue);
            const uint32_t stateValue = static_cast<uint32_t>(packet.state);
            out[5] = static_cast<uint8_t>(stateValue);
            const uint32_t reservedValue = static_cast<uint32_t>(packet.reserved);
            out[6] = static_cast<uint8_t>(reservedValue);
            out[7] = static_cast<uint8_t>(reservedValue >> 8);
            const uint32_t revisionValue = static_cast<uint32_t>(packet.revision);
            out[8] = static_cast<uint8_t>(revisionValue);
            out[9] = static_cast<uint8_t>(revisionValue >> 8);
            out[10] = static_cast<uint8_t>(revisionValue >> 16);
            out[11] = static_cast<uint8_t>(revisionValue >> 24);
            const uint32_t valueValue = static_cast<uint32_t>(packet.value);
            out[12] = static_cast<uint8_t>(valueValue);
            out[13] = static_cast<uint8_t>(valueValue >> 8);
            out[14] = static_cast<uint8_t>(valueValue >> 16);
            out[15] = static_cast<uint8_t>(valueValue >> 24);
            const uint32_t extraValue = static_cast<uint32_t>(packet.extra);
            out[16] = static_cast<uint8_t>(extraValue);
            out[17] = static_cast<uint8_t>(extraValue >> 8);
            out[18] = static_cast<uint8_t>(extraValue >> 16);
            out[19] = static_cast<uint8_t>(extraValue >> 24);
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DebugEvent readDebugEvent(const uint8_t *data) {
            DebugEvent packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t stateValue = (static_cast<uint32_t>(data[1]));
            packet.state = static_cast<uint8_t>(stateValue);
            const uint32_t codeValue = (static_cast<uint32_t>(data[2]));
            packet.code = static_cast<uint8_t>(codeValue);
            const uint32_t errorValue = (static_cast<uint32_t>(data[3]));
            packet.error = static_cast<uint8_t>(errorValue);
            const uint32_t stopIdValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.stopId = static_cast<uint32_t>(stopIdValue);
            const uint32_t revisionValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.revision = static_cast<uint32_t>(revisionValue);
            const uint32_t sizeValue = (static_cast<uint32_t>(data[12])) | (static_cast<uint32_t>(data[13]) << 8) | (static_cast<uint32_t>(data[14]) << 16) | (static_cast<uint32_t>(data[15]) << 24);
            packet.size = static_cast<uint32_t>(sizeValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[16])) | (static_cast<uint32_t>(data[17]) << 8) | (static_cast<uint32_t>(data[18]) << 16) | (static_cast<uint32_t>(data[19]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            return packet;
        }
        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeDebugEvent(uint8_t *out, const DebugEvent &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t stateValue = static_cast<uint32_t>(packet.state);
            out[1] = static_cast<uint8_t>(stateValue);
            const uint32_t codeValue = static_cast<uint32_t>(packet.code);
            out[2] = static_cast<uint8_t>(codeValue);
            const uint32_t errorValue = static_cast<uint32_t>(packet.error);
            out[3] = static_cast<uint8_t>(errorValue);
            const uint32_t stopIdValue = static_cast<uint32_t>(packet.stopId);
            out[4] = static_cast<uint8_t>(stopIdValue);
            out[5] = static_cast<uint8_t>(stopIdValue >> 8);
            out[6] = static_cast<uint8_t>(stopIdValue >> 16);
            out[7] = static_cast<uint8_t>(stopIdValue >> 24);
            const uint32_t revisionValue = static_cast<uint32_t>(packet.revision);
            out[8] = static_cast<uint8_t>(revisionValue);
            out[9] = static_cast<uint8_t>(revisionValue >> 8);
            out[10] = static_cast<uint8_t>(revisionValue >> 16);
            out[11] = static_cast<uint8_t>(revisionValue >> 24);
            const uint32_t sizeValue = static_cast<uint32_t>(packet.size);
            out[12] = static_cast<uint8_t>(sizeValue);
            out[13] = static_cast<uint8_t>(sizeValue >> 8);
            out[14] = static_cast<uint8_t>(sizeValue >> 16);
            out[15] = static_cast<uint8_t>(sizeValue >> 24);
            const uint32_t crcValue = static_cast<uint32_t>(packet.crc);
            out[16] = static_cast<uint8_t>(crcValue);
            out[17] = static_cast<uint8_t>(crcValue >> 8);
            out[18] = static_cast<uint8_t>(crcValue >> 16);
            out[19] = static_cast<uint8_t>(crcValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeSnapshotHeader(uint8_t *out, const SnapshotHeader &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t reasonValue = static_cast<uint32_t>(packet.reason);
            out[1] = static_cast<uint8_t>(reasonValue);
            const uint32_t slotBytesValue = static_cast<uint32_t>(packet.slotBytes);
            out[2] = static_cast<uint8_t>(slotBytesValue);
            out[3] = static_cast<uint8_t>(slotBytesValue >> 8);
            const uint32_t pcValue = static_cast<uint32_t>(packet.pc);
            out[4] = static_cast<uint8_t>(pcValue);
            out[5] = static_cast<uint8_t>(pcValue >> 8);
            out[6] = static_cast<uint8_t>(pcValue >> 16);
            out[7] = static_cast<uint8_t>(pcValue >> 24);
            const uint32_t bpValue = static_cast<uint32_t>(packet.bp);
            out[8] = static_cast<uint8_t>(bpValue);
            out[9] = static_cast<uint8_t>(bpValue >> 8);
            out[10] = static_cast<uint8_t>(bpValue >> 16);
            out[11] = static_cast<uint8_t>(bpValue >> 24);
            const uint32_t spValue = static_cast<uint32_t>(packet.sp);
            out[12] = static_cast<uint8_t>(spValue);
            out[13] = static_cast<uint8_t>(spValue >> 8);
            out[14] = static_cast<uint8_t>(spValue >> 16);
            out[15] = static_cast<uint8_t>(spValue >> 24);
            const uint32_t globalsValue = static_cast<uint32_t>(packet.globals);
            out[16] = static_cast<uint8_t>(globalsValue);
            out[17] = static_cast<uint8_t>(globalsValue >> 8);
            out[18] = static_cast<uint8_t>(globalsValue >> 16);
            out[19] = static_cast<uint8_t>(globalsValue >> 24);
            const uint32_t stackValue = static_cast<uint32_t>(packet.stack);
            out[20] = static_cast<uint8_t>(stackValue);
            out[21] = static_cast<uint8_t>(stackValue >> 8);
            out[22] = static_cast<uint8_t>(stackValue >> 16);
            out[23] = static_cast<uint8_t>(stackValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeSnapshotValue(uint8_t *out, const SnapshotValue &packet) {
            const uint32_t tagValue = static_cast<uint32_t>(packet.tag);
            out[0] = static_cast<uint8_t>(tagValue);
            const uint32_t reservedValue = static_cast<uint32_t>(packet.reserved);
            out[1] = static_cast<uint8_t>(reservedValue);
            const uint32_t classIdValue = static_cast<uint32_t>(packet.classId);
            out[2] = static_cast<uint8_t>(classIdValue);
            out[3] = static_cast<uint8_t>(classIdValue >> 8);
            const uint32_t payloadValue = static_cast<uint32_t>(packet.payload);
            out[4] = static_cast<uint8_t>(payloadValue);
            out[5] = static_cast<uint8_t>(payloadValue >> 8);
            out[6] = static_cast<uint8_t>(payloadValue >> 16);
            out[7] = static_cast<uint8_t>(payloadValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeStringHeader(uint8_t *out, const StringHeader &packet) {
            const uint32_t idValue = static_cast<uint32_t>(packet.id);
            out[0] = static_cast<uint8_t>(idValue);
            out[1] = static_cast<uint8_t>(idValue >> 8);
            out[2] = static_cast<uint8_t>(idValue >> 16);
            out[3] = static_cast<uint8_t>(idValue >> 24);
            const uint32_t lengthValue = static_cast<uint32_t>(packet.length);
            out[4] = static_cast<uint8_t>(lengthValue);
            out[5] = static_cast<uint8_t>(lengthValue >> 8);
            out[6] = static_cast<uint8_t>(lengthValue >> 16);
            out[7] = static_cast<uint8_t>(lengthValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeWord32(uint8_t *out, const Word32 &packet) {
            const uint32_t valueValue = static_cast<uint32_t>(packet.value);
            out[0] = static_cast<uint8_t>(valueValue);
            out[1] = static_cast<uint8_t>(valueValue >> 8);
            out[2] = static_cast<uint8_t>(valueValue >> 16);
            out[3] = static_cast<uint8_t>(valueValue >> 24);
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        Pc readPc(const uint8_t *data) {
            Pc packet{};
            const uint32_t valueValue = (static_cast<uint32_t>(data[0])) | (static_cast<uint32_t>(data[1]) << 8);
            packet.value = static_cast<uint16_t>(valueValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RemoteHeader readRemoteHeader(const uint8_t *data) {
            RemoteHeader packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t nonceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.nonce = static_cast<uint32_t>(nonceValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RemoteBegin readRemoteBegin(const uint8_t *data) {
            RemoteBegin packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t nonceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.nonce = static_cast<uint32_t>(nonceValue);
            const uint32_t totalValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8);
            packet.total = static_cast<uint16_t>(totalValue);
            const uint32_t crcValue = (static_cast<uint32_t>(data[10])) | (static_cast<uint32_t>(data[11]) << 8) | (static_cast<uint32_t>(data[12]) << 16) | (static_cast<uint32_t>(data[13]) << 24);
            packet.crc = static_cast<uint32_t>(crcValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RemoteData readRemoteData(const uint8_t *data) {
            RemoteData packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t nonceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.nonce = static_cast<uint32_t>(nonceValue);
            const uint32_t offsetValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8);
            packet.offset = static_cast<uint16_t>(offsetValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RemoteAck readRemoteAck(const uint8_t *data) {
            RemoteAck packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = (static_cast<uint32_t>(data[1]));
            packet.op = static_cast<uint8_t>(opValue);
            const uint32_t sequenceValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.sequence = static_cast<uint16_t>(sequenceValue);
            const uint32_t nonceValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8) | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
            packet.nonce = static_cast<uint32_t>(nonceValue);
            const uint32_t idValue = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) | (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);
            packet.id = static_cast<uint32_t>(idValue);
            return packet;
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeRemoteResult(uint8_t *out, const RemoteResult &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t opValue = static_cast<uint32_t>(packet.op);
            out[1] = static_cast<uint8_t>(opValue);
            const uint32_t sequenceValue = static_cast<uint32_t>(packet.sequence);
            out[2] = static_cast<uint8_t>(sequenceValue);
            out[3] = static_cast<uint8_t>(sequenceValue >> 8);
            const uint32_t nonceValue = static_cast<uint32_t>(packet.nonce);
            out[4] = static_cast<uint8_t>(nonceValue);
            out[5] = static_cast<uint8_t>(nonceValue >> 8);
            out[6] = static_cast<uint8_t>(nonceValue >> 16);
            out[7] = static_cast<uint8_t>(nonceValue >> 24);
            const uint32_t codeValue = static_cast<uint32_t>(packet.code);
            out[8] = static_cast<uint8_t>(codeValue);
            const uint32_t stateValue = static_cast<uint32_t>(packet.state);
            out[9] = static_cast<uint8_t>(stateValue);
            const uint32_t receivedValue = static_cast<uint32_t>(packet.received);
            out[10] = static_cast<uint8_t>(receivedValue);
            out[11] = static_cast<uint8_t>(receivedValue >> 8);
            const uint32_t reservedValue = static_cast<uint32_t>(packet.reserved);
            out[12] = static_cast<uint8_t>(reservedValue);
            out[13] = static_cast<uint8_t>(reservedValue >> 8);
            out[14] = static_cast<uint8_t>(reservedValue >> 16);
            out[15] = static_cast<uint8_t>(reservedValue >> 24);
            const uint32_t extraValue = static_cast<uint32_t>(packet.extra);
            out[16] = static_cast<uint8_t>(extraValue);
            out[17] = static_cast<uint8_t>(extraValue >> 8);
            out[18] = static_cast<uint8_t>(extraValue >> 16);
            out[19] = static_cast<uint8_t>(extraValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeRemoteEvent(uint8_t *out, const RemoteEvent &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t kindValue = static_cast<uint32_t>(packet.kind);
            out[1] = static_cast<uint8_t>(kindValue);
            const uint32_t nonceValue = static_cast<uint32_t>(packet.nonce);
            out[2] = static_cast<uint8_t>(nonceValue);
            out[3] = static_cast<uint8_t>(nonceValue >> 8);
            out[4] = static_cast<uint8_t>(nonceValue >> 16);
            out[5] = static_cast<uint8_t>(nonceValue >> 24);
            const uint32_t idValue = static_cast<uint32_t>(packet.id);
            out[6] = static_cast<uint8_t>(idValue);
            out[7] = static_cast<uint8_t>(idValue >> 8);
            out[8] = static_cast<uint8_t>(idValue >> 16);
            out[9] = static_cast<uint8_t>(idValue >> 24);
            const uint32_t objectValue = static_cast<uint32_t>(packet.object);
            out[10] = static_cast<uint8_t>(objectValue);
            out[11] = static_cast<uint8_t>(objectValue >> 8);
            const uint32_t methodValue = static_cast<uint32_t>(packet.method);
            out[12] = static_cast<uint8_t>(methodValue);
            out[13] = static_cast<uint8_t>(methodValue >> 8);
            const uint32_t argcValue = static_cast<uint32_t>(packet.argc);
            out[14] = static_cast<uint8_t>(argcValue);
            const uint32_t errorValue = static_cast<uint32_t>(packet.error);
            out[15] = static_cast<uint8_t>(errorValue);
            const uint32_t valueValue = static_cast<uint32_t>(packet.value);
            out[16] = static_cast<uint8_t>(valueValue);
            out[17] = static_cast<uint8_t>(valueValue >> 8);
            out[18] = static_cast<uint8_t>(valueValue >> 16);
            out[19] = static_cast<uint8_t>(valueValue >> 24);
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RegistryHeader readRegistryHeader(const uint8_t *data) {
            RegistryHeader packet{};
            const uint32_t versionValue = (static_cast<uint32_t>(data[0]));
            packet.version = static_cast<uint8_t>(versionValue);
            const uint32_t countValue = (static_cast<uint32_t>(data[1]));
            packet.count = static_cast<uint8_t>(countValue);
            const uint32_t reservedValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.reserved = static_cast<uint16_t>(reservedValue);
            packet.baseHash = data + 4;
            packet.rpcHash = data + 36;
            packet.registryHash = data + 68;
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        RegistryRecord readRegistryRecord(const uint8_t *data) {
            RegistryRecord packet{};
            const uint32_t nativeValue = (static_cast<uint32_t>(data[0])) | (static_cast<uint32_t>(data[1]) << 8);
            packet.native = static_cast<uint16_t>(nativeValue);
            const uint32_t objectValue = (static_cast<uint32_t>(data[2])) | (static_cast<uint32_t>(data[3]) << 8);
            packet.object = static_cast<uint16_t>(objectValue);
            const uint32_t methodValue = (static_cast<uint32_t>(data[4])) | (static_cast<uint32_t>(data[5]) << 8);
            packet.method = static_cast<uint16_t>(methodValue);
            const uint32_t classIdValue = (static_cast<uint32_t>(data[6])) | (static_cast<uint32_t>(data[7]) << 8);
            packet.classId = static_cast<uint16_t>(classIdValue);
            const uint32_t argcValue = (static_cast<uint32_t>(data[8]));
            packet.argc = static_cast<uint8_t>(argcValue);
            const uint32_t reservedValue = (static_cast<uint32_t>(data[9]));
            packet.reserved = static_cast<uint8_t>(reservedValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        ProvisionFrame readProvisionFrame(const uint8_t *data) {
            ProvisionFrame packet{};
            const uint32_t opValue = (static_cast<uint32_t>(data[0]));
            packet.op = static_cast<uint8_t>(opValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DeviceTag readDeviceTag(const uint8_t *data) {
            DeviceTag packet{};
            const uint32_t deviceTypeValue = (static_cast<uint32_t>(data[0]));
            packet.deviceType = static_cast<uint8_t>(deviceTypeValue);
            return packet;
        }

        /**
         * @brief 検査済み固定領域を復号する。
         * @param data 入力領域。
         * @return 値と借用bytes。
         */
        DeviceServo readDeviceServo(const uint8_t *data) {
            DeviceServo packet{};
            const uint32_t deviceTypeValue = (static_cast<uint32_t>(data[0]));
            packet.deviceType = static_cast<uint8_t>(deviceTypeValue);
            const uint32_t channelValue = (static_cast<uint32_t>(data[1]));
            packet.channel = static_cast<uint8_t>(channelValue);
            const uint32_t modeValue = (static_cast<uint32_t>(data[2]));
            packet.mode = static_cast<uint8_t>(modeValue);
            return packet;
        }





        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeTelemetryHeader(uint8_t *out, const TelemetryHeader &packet) {
            const uint32_t versionValue = static_cast<uint32_t>(packet.version);
            out[0] = static_cast<uint8_t>(versionValue);
            const uint32_t flagsValue = static_cast<uint32_t>(packet.flags);
            out[1] = static_cast<uint8_t>(flagsValue);
            const uint32_t sequenceValue = static_cast<uint32_t>(packet.sequence);
            out[2] = static_cast<uint8_t>(sequenceValue);
            out[3] = static_cast<uint8_t>(sequenceValue >> 8);
            const uint32_t millisValue = static_cast<uint32_t>(packet.millis);
            out[4] = static_cast<uint8_t>(millisValue);
            out[5] = static_cast<uint8_t>(millisValue >> 8);
            out[6] = static_cast<uint8_t>(millisValue >> 16);
            out[7] = static_cast<uint8_t>(millisValue >> 24);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeImuRecord(uint8_t *out, const ImuRecord &packet) {
            const uint32_t deviceTypeValue = static_cast<uint32_t>(packet.deviceType);
            out[0] = static_cast<uint8_t>(deviceTypeValue);
            const uint32_t accXValue = static_cast<uint32_t>(packet.accX);
            out[1] = static_cast<uint8_t>(accXValue);
            out[2] = static_cast<uint8_t>(accXValue >> 8);
            const uint32_t accYValue = static_cast<uint32_t>(packet.accY);
            out[3] = static_cast<uint8_t>(accYValue);
            out[4] = static_cast<uint8_t>(accYValue >> 8);
            const uint32_t accZValue = static_cast<uint32_t>(packet.accZ);
            out[5] = static_cast<uint8_t>(accZValue);
            out[6] = static_cast<uint8_t>(accZValue >> 8);
            const uint32_t gyroXValue = static_cast<uint32_t>(packet.gyroX);
            out[7] = static_cast<uint8_t>(gyroXValue);
            out[8] = static_cast<uint8_t>(gyroXValue >> 8);
            const uint32_t gyroYValue = static_cast<uint32_t>(packet.gyroY);
            out[9] = static_cast<uint8_t>(gyroYValue);
            out[10] = static_cast<uint8_t>(gyroYValue >> 8);
            const uint32_t gyroZValue = static_cast<uint32_t>(packet.gyroZ);
            out[11] = static_cast<uint8_t>(gyroZValue);
            out[12] = static_cast<uint8_t>(gyroZValue >> 8);
        }

        /**
         * @brief 固定領域を書く。
         * @param out 出力領域。
         * @param packet 検証済みの値。
         */
        void writeDistanceRecord(uint8_t *out, const DistanceRecord &packet) {
            const uint32_t deviceTypeValue = static_cast<uint32_t>(packet.deviceType);
            out[0] = static_cast<uint8_t>(deviceTypeValue);
            const uint32_t distanceMmValue = static_cast<uint32_t>(packet.distanceMm);
            out[1] = static_cast<uint8_t>(distanceMmValue);
            out[2] = static_cast<uint8_t>(distanceMmValue >> 8);
        }

    }
}
namespace uroa {
    namespace wire {
        bool validDeviceServo(const DeviceServo &packet) {
            return (packet.channel <= 15) && (packet.mode <= 1);
        }
        bool validTelemetryHeader(const TelemetryHeader &packet) {
            return (packet.version == 3);
        }
        bool validDistanceRecord(const DistanceRecord &packet) {
            return (packet.distanceMm == 65535 || (packet.distanceMm >= 20 && packet.distanceMm <= 4000));
        }
        size_t DeviceInfoRecordsRecordLength(uint8_t kind) {
            switch (kind) {
                case 0:
                    return DeviceTagLayout::ByteLength;
                case 1:
                    return DeviceTagLayout::ByteLength;
                case 2:
                    return DeviceTagLayout::ByteLength;
                case 3:
                    return DeviceTagLayout::ByteLength;
                case 128:
                    return DeviceServoLayout::ByteLength;
                default:
                    return 0;
            }
        }
        size_t TelemetryRecordsRecordLength(uint8_t kind) {
            switch (kind) {
                case 0:
                    return ImuRecordLayout::ByteLength;
                case 2:
                    return DistanceRecordLayout::ByteLength;
                default:
                    return 0;
            }
        }
        size_t TelemetryRecordsLength(const uint8_t *kinds, size_t count) {
            if (count && !kinds) {
                return 0;
            }
            size_t length = 8;
            for (size_t index = 0; index < count; ++index) {
                const size_t bytes = TelemetryRecordsRecordLength(kinds[index]);
                if (!bytes || length > SIZE_MAX - bytes) {
                    return 0;
                }
                length += bytes;
            }
            return length;
        }
        size_t writeTelemetryRecords(uint8_t *out, size_t capacity, const TelemetryHeader &header, const uint8_t *kinds, size_t count, const TelemetryRecordsValues &values) {
            const size_t length = TelemetryRecordsLength(kinds, count);
            if (!out || !length || capacity < length || !validTelemetryHeader(header)) {
                return 0;
            }
            for (size_t index = 0; index < count; ++index) {
                switch (kinds[index]) {
                    case 0:
                        if (values.imuRecord.deviceType != 0) {
                            return 0;
                        }
                        break;
                    case 2:
                        if (values.distanceRecord.deviceType != 2 || !validDistanceRecord(values.distanceRecord)) {
                            return 0;
                        }
                        break;
                }
            }
            writeTelemetryHeader(out, header);
            size_t offset = 8;
            for (size_t index = 0; index < count; ++index) {
                switch (kinds[index]) {
                    case 0:
                        writeImuRecord(out + offset, values.imuRecord);
                        break;
                    case 2:
                        writeDistanceRecord(out + offset, values.distanceRecord);
                        break;
                }
                offset += TelemetryRecordsRecordLength(kinds[index]);
            }
            return offset;
        }
    }
}
