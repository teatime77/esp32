/**
 * @file uroa_protocol.h
 * @brief diagramと共有するprotocol v1と機器種別付きTelemetryのパケットを読み書きする。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace uroa {
    /**
     * @brief 通信や機器の状態を持たないバイナリ形式の変換処理。
     * @details 指令・Statusはversion 1、機器種別付きTelemetryはversion 3。角度は0.01度、時間はms、数値はlittle endian。通信・機器の状態を持たず、パケットの読み書きだけを担当する。
     */
    class Protocol {
        public:
        static constexpr uint8_t DeviceTelemetryVersion = 3; ///< 機器種別付きTelemetryの版番号。
        static constexpr size_t DeviceTelemetryHeaderLength = 8; ///< version・flags・sequence・millisの長さ。
        static constexpr size_t IMU6AxisRecordLength = 13; ///< 種別1バイトと加速度・角速度の6軸。
        static constexpr uint8_t IMU6AxisDeviceType = 0; ///< diagramのDeviceType::IMU6Axisと共通。
        /**
         * @brief protocol v1の指令操作番号。
         */
        enum Op : uint8_t {
            ARM = 1,
            MOVE = 2,
            STOP = 3,
            STREAM = 4,
            PING = 5,
            INFO = 6,
            DISABLE = 7,
            ARM_SPEED = 8,
            SET_SPEED = 9
        };

        /**
         * @brief Status通知の受付結果コード。
         */
        enum Code : uint8_t {
            CODE_OK = 0,
            CODE_FORMAT = 1,
            CODE_RANGE = 2,
            CODE_NOT_ARMED = 3,
            CODE_HARDWARE = 4,
            CODE_BUSY = 5,
            CODE_IO_ERROR = 6,
            CODE_MODE = 7
        };

        /**
         * @brief version、操作、sequence、CH、値と時間を含む10バイトの指令。
         */
        struct Command {
            uint8_t bytes[10]{};
        };

        /**
         * @brief little endianの16bit値を読み取る。
         * @param bytes バイト配列。
         * @return 符号なし16bit値。
         */
        static uint16_t read16(const uint8_t *bytes);
        /** @brief little endianの符号付き16bit値を読む。@param bytes 先頭バイト。@return 符号付き値。 */
        static int16_t readSigned16(const uint8_t *bytes);
        static constexpr uint8_t FLAG_SPEED = 0x10; ///< StatusのCHが速度モードなら1。値は符号付き0.01%。

        /**
         * @brief 16bit値をlittle endianで書き込む。
         * @param bytes バイト配列。
         * @param value 検査または変換する値。
         */
        static void write16(uint8_t *bytes, uint16_t value);
        /**
         * @brief 32bit値をlittle endianで書き込む。
         * @param bytes バイト配列。
         * @param value 検査または変換する値。
         */
        static void write32(uint8_t *bytes, uint32_t value);
        /**
         * @brief 受信データを固定長の指令へコピーする。
         * @details 長さ不正はversion=0の指令へ変換し、通常の形式エラー応答に回す。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 長さが不正な場合はversion=0の指令。
         */
        static Command command(const uint8_t *data, size_t length);
        /**
         * @brief 指令のversion、予約バイトと操作番号を検査する。
         * @param command 処理対象の指令。
         * @return 指令形式が正しければtrue。値の範囲は教材側で検査する。
         */
        static bool valid(const Command &command);
        /**
         * @brief 通常キューを迂回するSTOP指令か検査する。
         * @details STOPは通常キューを迂回する。従来どおりversionとopcodeだけを検査する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 長さ・version・操作番号がSTOPに一致すればtrue。
         */
        static bool urgentStop(const uint8_t *data, size_t length);
        /**
         * @brief 機器の状態と受付結果を12バイトのStatusへ詰める。
         * @param out 出力先バッファーまたはJSONオブジェクト。
         * @param code Statusの受付結果コード。
         * @param sequence 16bitで周回するsequence。
         * @param flags 機器・制御・通知の状態フラグ。
         * @param channel 出力CH番号。
         * @param current 角度は0.01度。FLAG_SPEED時は符号付き0.01%のビット列。
         * @param target 角度の目標値、またはFLAG_SPEED時は速度指令値のビット列。
         * @param remaining 補間の残り時間（ms）。
         */
        static void status(uint8_t *out, uint8_t code, uint16_t sequence, uint8_t flags, uint8_t channel = 255,
            uint16_t current = 0, uint16_t target = 0, uint16_t remaining = 0);
        /**
         * @brief 登録された6軸IMUの数だけ種別付きレコードを連結する。
         * @param out 出力バッファー。容量不足なら変更しない。
         * @param capacity 出力領域のバイト数。
         * @param flags 機器・制御・通知の状態フラグ。
         * @param sequence 通知のsequence。
         * @param now 取得時刻millis。
         * @param raw MPU-6050の14バイト。加速度・温度・角速度の順。
         * @param imuCount 受信順に並べるIMU6Axisレコードの数。同じ物理IMUの値を共有する。
         * @return 出力バイト数。容量不足なら0。
         */
        static size_t deviceTelemetry(uint8_t *out, size_t capacity, uint8_t flags, uint16_t sequence, uint32_t now,
            const uint8_t *raw, size_t imuCount);
    };
}
