/**
 * @file uroa_protocol.h
 * @brief diagramと共有するprotocol v1と機器種別付きTelemetryのパケットを読み書きする。
 * @details 操作番号・固定長配置・Command復号/通常形式検査・Status符号化は
 * uroa/diagram/rpc/device.protoを正本とし、uroa_rpc.generated.h/.cppから使う。
 * main.cppは復号した値をloop()のcontrol_rpc.generated.cppへ渡し、機能サービスを呼ぶ。
 * 優先STOPの生バイト判定は通常形式検査と分けて維持する。
 * バイナリは既存形式を維持し、API用の番号や指紋をパケットへ追加しない。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "uroa_rpc.generated.h"
#include "uroa_wire.generated.h"

namespace uroa {
    /**
     * @brief 通信や機器の状態を持たないバイナリ形式の変換処理。
     * @details 指令・Statusはversion 1、機器種別付きTelemetryはversion 3。角度は0.01度、時間はms、数値はlittle endian。通信・機器の状態を持たず、パケットの読み書きだけを担当する。
     */
    class Protocol {
        public:
        static constexpr uint8_t DeviceTelemetryVersion = wire::TelemetryHeaderLayout::versionDefault; ///< 機器種別付きTelemetryの版番号。
        static constexpr size_t DeviceTelemetryHeaderLength =
            rpc::TelemetryHeaderLayout::ByteLength; ///< version・flags・sequence・millisの長さ。
        static constexpr size_t IMU6AxisRecordLength =
            rpc::ImuRecordLayout::ByteLength; ///< 種別1バイトと加速度・角速度の6軸。
        static constexpr uint8_t IMU6AxisDeviceType = 0; ///< diagramのDeviceType::IMU6Axisと共通。
        static constexpr size_t UltrasonicRecordLength =
            rpc::DistanceRecordLayout::ByteLength; ///< 種別1バイトと距離mmのuint16。
        static constexpr uint8_t UltrasonicDeviceType = 2; ///< diagramのDeviceType::UltrasonicDistanceSensorと共通。
        static constexpr uint16_t InvalidDistanceMm = wire::DistanceRecordLayout::distanceMmSentinel; ///< 距離の未測定・測定失敗を表す予約値。
        static constexpr uint16_t MinDistanceMm = wire::DistanceRecordLayout::distanceMmMin; ///< 有効距離の下限（mm）。
        static constexpr uint16_t MaxDistanceMm = wire::DistanceRecordLayout::distanceMmMax; ///< 有効距離の上限（mm）。

        /**
         * @brief protocol v1の指令操作番号。
         * @details VM_START/VM_STOPはCH=255、value=duration=0。diagram-controlのmain.cppが
         * 保持済みコードの明示開始／VMと所有機器の停止を行う。
         * 一括転送は開始フラグがなければ保持まで。開始フラグ付きCOMMITは全体検証・PC設定後に別経路で開始する。
         * 転送契約はuroa/diagram/ts/ble-wifi/bytecode-transfer.tsを参照する。
         * 開始ACKはVM初期化までを表し、実行終了は表さない。Web側は
         * uroa/diagram/ts/ble-wifi/device-connection.tsとpython/python-block.tsを参照する。
         */
        enum Op : uint8_t {
            ARM = static_cast<uint8_t>(rpc::CommandOpcode::ARM),
            MOVE = static_cast<uint8_t>(rpc::CommandOpcode::MOVE),
            STOP = static_cast<uint8_t>(rpc::CommandOpcode::STOP),
            STREAM = static_cast<uint8_t>(rpc::CommandOpcode::STREAM),
            PING = static_cast<uint8_t>(rpc::CommandOpcode::PING),
            INFO = static_cast<uint8_t>(rpc::CommandOpcode::INFO),
            DISABLE = static_cast<uint8_t>(rpc::CommandOpcode::DISABLE),
            ARM_SPEED = static_cast<uint8_t>(rpc::CommandOpcode::ARM_SPEED),
            SET_SPEED = static_cast<uint8_t>(rpc::CommandOpcode::SET_SPEED),
            VM_START = static_cast<uint8_t>(rpc::CommandOpcode::VM_START),
            VM_STOP = static_cast<uint8_t>(rpc::CommandOpcode::VM_STOP)
        };

        /**
         * @brief Status通知の受付結果コード。
         */
        enum Code : uint8_t {
            CODE_OK = static_cast<uint8_t>(rpc::StatusCode::CODE_OK),
            CODE_FORMAT = static_cast<uint8_t>(rpc::StatusCode::CODE_FORMAT),
            CODE_RANGE = static_cast<uint8_t>(rpc::StatusCode::CODE_RANGE),
            CODE_NOT_ARMED = static_cast<uint8_t>(rpc::StatusCode::CODE_NOT_ARMED),
            CODE_HARDWARE = static_cast<uint8_t>(rpc::StatusCode::CODE_HARDWARE),
            CODE_BUSY = static_cast<uint8_t>(rpc::StatusCode::CODE_BUSY),
            CODE_IO_ERROR = static_cast<uint8_t>(rpc::StatusCode::CODE_IO_ERROR),
            CODE_MODE = static_cast<uint8_t>(rpc::StatusCode::CODE_MODE)
        };

        /**
         * @brief 10バイトの指令から生成codecで復号した値。メモリー配置を通信サイズに使わない。
         */
        using Command = rpc::CommandPacket;

        /**
         * @brief little endianの16bit値を読み取る。
         * @param bytes バイト配列。
         * @return 符号なし16bit値。
         */
        static uint16_t read16(const uint8_t *bytes);
        /** @brief little endianの符号付き16bit値を読む。@param bytes 先頭バイト。@return 符号付き値。 */
        static int16_t readSigned16(const uint8_t *bytes);
        static constexpr uint8_t FLAG_SPEED = rpc::StatusPacketLayout::SpeedFlag; ///< 速度モード。値は符号付き0.01%。

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
         * @brief 受信データを生成codecで指令の値へ復号する。
         * @details nullptr・長さ不正は全フィールド0にして通常の形式エラー応答に回す。
         * 全長一致時は不正な版・予約値・opcodeでもsequence等を保持し、valid()で形式を検査する。
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
         * @brief センサー種別の並びからversion 3の通知長を計算する。
         * @param sensorTypes DeviceInfo順のセンサー種別。サーボは含めない。件数0ならnullptrも可。
         * @param sensorCount センサーの件数。0ならヘッダー長だけを返す。
         * @return 必要なバイト数。不正な配列・未対応種別・サイズのオーバーフローでは0。
         */
        static size_t deviceTelemetryLength(const uint8_t *sensorTypes, size_t sensorCount);
        /**
         * @brief DeviceInfo順のIMU・超音波距離レコードを連結する。測定・通信は行わない。
         * @param out 出力バッファー。引数不正・容量不足なら変更しない。
         * @param capacity 出力領域のバイト数。
         * @param flags 機器・制御・通知の状態フラグ。
         * @param sequence 通知のsequence。
         * @param now 通知を組み立てるサンプル時刻millis。距離は直近の別周期の測定値。
         * @param sensorTypes DeviceInfo順のセンサー種別。サーボは含めない。
         * @param sensorCount センサーの件数。
         * @param imuRaw MPU-6050の14バイト。同種別のレコードへ共有する。IMUがなければnullptrも可。
         * @param distanceMm 20〜4000mm、またはInvalidDistanceMm。同種別のレコードへ共有する。
         * @return 出力バイト数。引数不正・未対応種別・容量不足なら0。
         */
        static size_t deviceTelemetry(uint8_t *out, size_t capacity, uint8_t flags, uint16_t sequence, uint32_t now,
            const uint8_t *sensorTypes, size_t sensorCount, const uint8_t *imuRaw, uint16_t distanceMm);
    };
}
