/**
 * @file uroa_protocol.h
 * @brief diagramと共有するprotocol v1と機器種別付きTelemetryのパケットを読み書きする。
 * @details Command/Statusの番号・型・codecはuroa_rpc.generated.h/.cppを直接使う。
 * 本クラスは優先STOPの判定と、測定値を生成Telemetry列へ渡す処理だけを持つ。
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
        static constexpr uint8_t DeviceTelemetryVersion =
            wire::TelemetryHeaderLayout::versionDefault; ///< 機器種別付きTelemetryの版番号。
        static constexpr size_t DeviceTelemetryHeaderLength =
            wire::TelemetryHeaderLayout::ByteLength; ///< version・flags・sequence・millisの長さ。
        static constexpr size_t IMU6AxisRecordLength =
            wire::ImuRecordLayout::ByteLength; ///< 種別1バイトと加速度・角速度の6軸。
        static constexpr uint8_t IMU6AxisDeviceType =
            wire::DeviceKind::IMU6Axis; ///< diagramのDeviceType::IMU6Axisと共通。
        static constexpr size_t UltrasonicRecordLength =
            wire::DistanceRecordLayout::ByteLength; ///< 種別1バイトと距離mmのuint16。
        static constexpr uint8_t UltrasonicDeviceType =
            wire::DeviceKind::UltrasonicDistanceSensor; ///< diagramのDeviceType::UltrasonicDistanceSensorと共通。
        static constexpr uint16_t InvalidDistanceMm =
            wire::DistanceRecordLayout::distanceMmSentinel; ///< 距離の未測定・測定失敗を表す予約値。
        static constexpr uint16_t MinDistanceMm = wire::DistanceRecordLayout::distanceMmMin; ///< 有効距離の下限（mm）。
        static constexpr uint16_t MaxDistanceMm = wire::DistanceRecordLayout::distanceMmMax; ///< 有効距離の上限（mm）。

        /**
         * @brief 通常キューを迂回するSTOP指令か検査する。
         * @details STOPは通常キューを迂回する。従来どおりversionとopcodeだけを検査する。
         * @param data 入力または出力のバイト配列。
         * @param length データ長（バイト）。
         * @return 長さ・version・操作番号がSTOPに一致すればtrue。
         */
        static bool urgentStop(const uint8_t *data, size_t length);
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
