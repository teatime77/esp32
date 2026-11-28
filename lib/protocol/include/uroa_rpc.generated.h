/**
 * @file uroa_rpc.generated.h
 * @brief device.protoから生成した既存パケットの番号・配置・方向別codec。手動編集しない。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace uroa {
    namespace rpc {
        constexpr uint32_t ApiVersion = 1; ///< 呼出しAPIの版。既存Commandの版とは別。
        constexpr char SchemaFingerprint[] = "99d8eab721a50d2deda1e415c268665dc3730d66c80a19104a87439d890d8730"; ///< 生成物整合用。現在の通信で交換しない。
        /**
         * @brief 既存パケットの操作・結果番号。
         */
        enum class CommandOpcode : uint8_t {
            COMMAND_UNSPECIFIED = 0,
            ARM = 1,
            MOVE = 2,
            STOP = 3,
            STREAM = 4,
            PING = 5,
            INFO = 6,
            DISABLE = 7,
            ARM_SPEED = 8,
            SET_SPEED = 9,
            VM_START = 10,
            VM_STOP = 11,
        };

        /**
         * @brief 既存パケットの操作・結果番号。
         */
        enum class StatusCode : uint8_t {
            CODE_OK = 0,
            CODE_FORMAT = 1,
            CODE_RANGE = 2,
            CODE_NOT_ARMED = 3,
            CODE_HARDWARE = 4,
            CODE_BUSY = 5,
            CODE_IO_ERROR = 6,
            CODE_MODE = 7,
        };

        /**
         * @brief ローカルRPCのサービスと操作の識別番号。既存パケットへは追加しない。
         */
        enum class Method : uint32_t {
            ConnectionConnect = 65537,
            ConnectionDisconnect = 65538,
            DeviceRegister = 131073,
            DeviceInfo = 131074,
            DevicePing = 131075,
            DeviceWatch = 131076,
            ServoArm = 196609,
            ServoMove = 196610,
            ServoSpeed = 196611,
            ServoDisable = 196612,
            ServoStop = 196613,
            ProgramUpload = 262145,
            ProgramStart = 262146,
            ProgramStop = 262147,
            DebugBreakpoints = 327681,
            DebugObserve = 327682,
            DebugExecute = 327683,
            DebugWatch = 327684,
            WifiOpen = 393217,
            WifiClose = 393218,
            WifiInfo = 393219,
            WifiScan = 393220,
            WifiConfigure = 393221,
            WifiForget = 393222,
            WifiConnect = 393223,
            CameraCapture = 458753,
            RemoteOpen = 524289,
            RemoteSessionConfigure = 589825,
            RemoteSessionRead = 589826,
            RemoteSessionAcknowledge = 589827,
            RemoteSessionEnd = 589828,
            SensorsSubscribe = 655361,
            ControlStream = 720897,
        };

        /**
         * @brief 宣言順・little endian・パディングなしの位置。sizeofで通信サイズを求めない。
         */
        struct CommandPacketLayout {
            static constexpr size_t ByteLength = 10; ///< パケット全長。
            static constexpr size_t version = 0; ///< 8bitの先頭位置。
            static constexpr uint8_t versionValue = 1; ///< 通常形式の固定値。
            static constexpr size_t opcode = 1; ///< 8bitの先頭位置。
            static constexpr size_t sequence = 2; ///< 16bitの先頭位置。
            static constexpr size_t channel = 4; ///< 8bitの先頭位置。
            static constexpr size_t reserved = 5; ///< 8bitの先頭位置。
            static constexpr uint8_t reservedValue = 0; ///< 通常形式の固定値。
            static constexpr size_t value = 6; ///< 16bitの先頭位置。
            static constexpr size_t duration = 8; ///< 16bitの先頭位置。
        };

        /**
         * @brief 固定長パケットの値。メモリー上の配置を直接送らない。
         */
        struct CommandPacket {
            uint8_t version; ///< 8bitの値。
            uint8_t opcode; ///< 8bitの値。
            uint16_t sequence; ///< 16bitの値。
            uint8_t channel; ///< 8bitの値。
            uint8_t reserved; ///< 8bitの値。
            uint16_t value; ///< 16bitの値。
            uint16_t duration; ///< 16bitの値。
        };

        /**
         * @brief 全長が一致するパケットを読む。形式検査は別途行う。
         * @param data 入力バイト列。
         * @param length 入力のバイト数。
         * @return 長さ不正またはnullptrなら全フィールド0、それ以外は復号した値。
         */
        CommandPacket decodeCommandPacket(const uint8_t *data, size_t length);

        /**
         * @brief 固定値・enum・下限による通常の形式検査。機器の範囲検査は含まない。
         * @param packet 復号した値。
         * @return 形式が正しければtrue。
         */
        bool validateCommandPacket(const CommandPacket &packet);

        /**
         * @brief 宣言順・little endian・パディングなしの位置。sizeofで通信サイズを求めない。
         */
        struct StatusPacketLayout {
            static constexpr size_t ByteLength = 12; ///< パケット全長。
            static constexpr size_t version = 0; ///< 8bitの先頭位置。
            static constexpr uint8_t versionValue = 1; ///< 通常形式の固定値。
            static constexpr size_t code = 1; ///< 8bitの先頭位置。
            static constexpr size_t sequence = 2; ///< 16bitの先頭位置。
            static constexpr size_t flags = 4; ///< 8bitの先頭位置。
            static constexpr size_t channel = 5; ///< 8bitの先頭位置。
            static constexpr size_t current = 6; ///< 16bitの先頭位置。
            static constexpr size_t target = 8; ///< 16bitの先頭位置。
            static constexpr size_t remaining = 10; ///< 16bitの先頭位置。
            static constexpr uint16_t ChannelCount = 16; ///< 値を持つCHの件数。
            static constexpr uint8_t SpeedFlag = 16; ///< 速度モードの状態ビット。
            static constexpr uint32_t SpeedScale = 100; ///< 速度指令の整数化倍率。
        };

        /**
         * @brief 固定長パケットの値。メモリー上の配置を直接送らない。
         */
        struct StatusPacket {
            uint8_t version; ///< 8bitの値。
            uint8_t code; ///< 8bitの値。
            uint16_t sequence; ///< 16bitの値。
            uint8_t flags; ///< 8bitの値。
            uint8_t channel; ///< 8bitの値。
            uint16_t current; ///< 16bitの値。
            uint16_t target; ///< 16bitの値。
            uint16_t remaining; ///< 16bitの値。
        };

        /**
         * @brief 宣言順に固定長パケットを書く。
         * @param out ByteLength以上の出力領域。
         * @param packet 呼出元が確定した幅・固定値の値。
         */
        void encodeStatusPacket(uint8_t *out, const StatusPacket &packet);

        /**
         * @brief 宣言順・little endian・パディングなしの位置。sizeofで通信サイズを求めない。
         */
        struct TelemetryHeaderLayout {
            static constexpr size_t ByteLength = 8; ///< パケット全長。
            static constexpr size_t version = 0; ///< 8bitの先頭位置。
            static constexpr uint8_t versionValue = 3; ///< 通常形式の固定値。
            static constexpr size_t flags = 1; ///< 8bitの先頭位置。
            static constexpr size_t sequence = 2; ///< 16bitの先頭位置。
            static constexpr size_t millis = 4; ///< 32bitの先頭位置。
        };

        /**
         * @brief 固定長パケットの値。メモリー上の配置を直接送らない。
         */
        struct TelemetryHeader {
            uint8_t version; ///< 8bitの値。
            uint8_t flags; ///< 8bitの値。
            uint16_t sequence; ///< 16bitの値。
            uint32_t millis; ///< 32bitの値。
        };

        /**
         * @brief 宣言順・little endian・パディングなしの位置。sizeofで通信サイズを求めない。
         */
        struct ImuRecordLayout {
            static constexpr size_t ByteLength = 13; ///< パケット全長。
            static constexpr size_t deviceType = 0; ///< 8bitの先頭位置。
            static constexpr size_t accX = 1; ///< 16bitの先頭位置。
            static constexpr size_t accY = 3; ///< 16bitの先頭位置。
            static constexpr size_t accZ = 5; ///< 16bitの先頭位置。
            static constexpr size_t gyroX = 7; ///< 16bitの先頭位置。
            static constexpr size_t gyroY = 9; ///< 16bitの先頭位置。
            static constexpr size_t gyroZ = 11; ///< 16bitの先頭位置。
        };

        /**
         * @brief 固定長パケットの値。メモリー上の配置を直接送らない。
         */
        struct ImuRecord {
            uint8_t deviceType; ///< 8bitの値。
            int16_t accX; ///< 16bitの値。
            int16_t accY; ///< 16bitの値。
            int16_t accZ; ///< 16bitの値。
            int16_t gyroX; ///< 16bitの値。
            int16_t gyroY; ///< 16bitの値。
            int16_t gyroZ; ///< 16bitの値。
        };

        /**
         * @brief 宣言順・little endian・パディングなしの位置。sizeofで通信サイズを求めない。
         */
        struct DistanceRecordLayout {
            static constexpr size_t ByteLength = 3; ///< パケット全長。
            static constexpr size_t deviceType = 0; ///< 8bitの先頭位置。
            static constexpr size_t distanceMm = 1; ///< 16bitの先頭位置。
        };

        /**
         * @brief 固定長パケットの値。メモリー上の配置を直接送らない。
         */
        struct DistanceRecord {
            uint8_t deviceType; ///< 8bitの値。
            uint16_t distanceMm; ///< 16bitの値。
        };

        /**
         * @brief 固定値とCHの条件に従ってStatusの値を作る。
         * @param code 受付結果。
         * @param sequence 応答対象。
         * @param flags 機器状態。
         * @param channel CHまたは全体。
         * @param current 現在の指令値のビット列。
         * @param target 目標値のビット列。
         * @param remaining 残り時間ms。
         * @return 通信へ書き込む値。全体CHの3値は0。
         */
        StatusPacket makeStatusPacket(uint8_t code, uint16_t sequence, uint8_t flags, uint8_t channel,
            uint16_t current, uint16_t target, uint16_t remaining);

    }
}
