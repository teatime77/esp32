/**
 * @file uroa_wire.generated.h
 * @brief .protoの部分領域codec。手動編集しない。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace uroa {
    namespace wire {
        namespace GattUuidValues {
            constexpr char service[] = "f3641400-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char command[] = "f3641401-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char telemetry[] = "f3641402-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char status[] = "f3641403-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char wifiConfig[] = "f3641404-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char wifiResult[] = "f3641405-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char deviceInfo[] = "f3641406-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char bytecode[] = "f3641407-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char bytecodeResult[] = "f3641408-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char debugControl[] = "f3641409-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char debugResult[] = "f364140a-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char debugEvent[] = "f364140b-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char debugData[] = "f364140c-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char rpcControl[] = "f364140d-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char rpcResult[] = "f364140e-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
            constexpr char rpcEvent[] = "f364140f-00b0-4240-ba50-05ca45bf8abc"; ///< 既存GATT UUID。
        }
        namespace TransferOperation {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                None = 0,
                Begin = 1,
                Data = 2,
                Commit = 3,
                Abort = 4,
            };
        }
        namespace TransferCode {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Ok = 0,
                Format = 1,
                Order = 2,
                Range = 3,
                Memory = 4,
                Crc = 5,
                Invalid = 6,
                Timeout = 7,
                Busy = 8,
                Start = 9,
            };
        }
        namespace TransferState {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Idle = 0,
                Receiving = 1,
                Ready = 2,
                Failed = 3,
                Started = 4,
            };
        }
        namespace DebugCode {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Ok = 0,
                Format = 1,
                Order = 2,
                Range = 3,
                Memory = 4,
                Crc = 5,
                Invalid = 6,
                Busy = 7,
                Stale = 8,
                Timeout = 9,
            };
        }
        namespace DebugOperation {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                None = 0,
                Begin = 1,
                Data = 2,
                Commit = 3,
                Query = 4,
                Chunk = 5,
                Clear = 6,
                PointsBegin = 7,
                PointsData = 8,
                PointsCommit = 9,
                Continue = 10,
                Over = 11,
                In = 12,
                Out = 13,
            };
        }
        namespace RemoteCode {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Ok = 0,
                Format = 1,
                Order = 2,
                Range = 3,
                Busy = 4,
                Mismatch = 5,
                Timeout = 6,
            };
        }
        namespace RemoteOperation {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                None = 0,
                Begin = 1,
                Data = 2,
                Commit = 3,
                Ack = 4,
                End = 5,
            };
        }
        namespace RemoteState {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Unregistered = 0,
                Receiving = 1,
                Configured = 2,
            };
        }
        namespace RemoteKind {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Empty = 0,
                Pending = 1,
                Fault = 2,
            };
        }
        namespace ProvisionOperation {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                Reset = 0,
                Append = 1,
                Commit = 2,
            };
        }
        namespace DeviceKind {
            /**
             * @brief 既存通信の番号。生バイトの値はcodecが保持し、意味は所有者が検査する。
             */
            enum Value : uint8_t {
                IMU6Axis = 0,
                IMU9Axis = 1,
                UltrasonicDistanceSensor = 2,
                Camera = 3,
                ServoMotor = 128,
            };
        }
        namespace RegistryHeaderLimits {
            constexpr size_t BaseHash = 32; ///< .protoの最大長・件数。
            constexpr size_t RpcHash = 32; ///< .protoの最大長・件数。
            constexpr size_t RegistryHash = 32; ///< .protoの最大長・件数。
        }
        namespace ProgramFrameLimits {
            constexpr size_t Payload = 1014; ///< .protoの最大長・件数。
        }
        namespace ProgramBundleLimits {
            constexpr size_t Image = 131072; ///< .protoの最大長・件数。
            constexpr size_t Breakpoints = 4096; ///< .protoの最大長・件数。
            constexpr size_t Points = 65536; ///< .protoの最大長・件数。
        }
        namespace DebugFrameLimits {
            constexpr size_t Payload = 12; ///< .protoの最大長・件数。
        }
        namespace RemoteFrameLimits {
            constexpr size_t Payload = 12; ///< .protoの最大長・件数。
        }
        namespace RegistryLimits {
            constexpr size_t Records = 32; ///< .protoの最大長・件数。
        }
        namespace SnapshotStringLimits {
            constexpr size_t Utf8 = 262144; ///< .protoの最大長・件数。
        }
        namespace SnapshotBodyLimits {
            constexpr size_t Values = 2048; ///< .protoの最大長・件数。
            constexpr size_t Strings = 2048; ///< .protoの最大長・件数。
        }
        namespace ProvisionBodyLimits {
            constexpr size_t JsonUtf8 = 383; ///< .protoの最大長・件数。
        }
        /**
         * @brief TransferHeaderの既存wire配置。
         */
        struct TransferHeaderLayout {
            static constexpr size_t ByteLength = 10; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 2; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t transfer = 2; ///< フィールド位置。
            static constexpr size_t sequence = 4; ///< フィールド位置。
            static constexpr size_t position = 6; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct TransferHeader {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t transfer; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t position; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 10バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        TransferHeader readTransferHeader(const uint8_t *data);

        /**
         * @brief TransferBeginの既存wire配置。
         */
        struct TransferBeginLayout {
            static constexpr size_t ByteLength = 14; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 2; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t transfer = 2; ///< フィールド位置。
            static constexpr size_t sequence = 4; ///< フィールド位置。
            static constexpr size_t position = 6; ///< フィールド位置。
            static constexpr size_t crc = 10; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct TransferBegin {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t transfer; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t position; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 14バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        TransferBegin readTransferBegin(const uint8_t *data);

        /**
         * @brief TransferResultの既存wire配置。
         */
        struct TransferResultLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 2; ///< 宣言した既定値。
            static constexpr size_t code = 1; ///< フィールド位置。
            static constexpr size_t transfer = 2; ///< フィールド位置。
            static constexpr size_t frame = 4; ///< フィールド位置。
            static constexpr size_t op = 6; ///< フィールド位置。
            static constexpr size_t state = 7; ///< フィールド位置。
            static constexpr size_t received = 8; ///< フィールド位置。
            static constexpr size_t revision = 12; ///< フィールド位置。
            static constexpr size_t crc = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct TransferResult {
            uint8_t version; ///< wire上の値。
            uint8_t code; ///< wire上の値。
            uint16_t transfer; ///< wire上の値。
            uint16_t frame; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint8_t state; ///< wire上の値。
            uint32_t received; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 20バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        TransferResult readTransferResult(const uint8_t *data);
        /**
         * @brief 固定長の値を書き込む。
         * @param out 20バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeTransferResult(uint8_t *out, const TransferResult &packet);

        /**
         * @brief BundleHeaderの既存wire配置。
         */
        struct BundleHeaderLayout {
            static constexpr size_t ByteLength = 24; ///< 固定領域の長さ。
            static constexpr size_t signature = 0; ///< フィールド位置。
            static constexpr uint32_t signatureDefault = 844255317; ///< 宣言した既定値。
            static constexpr size_t version = 4; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t flags = 6; ///< フィールド位置。
            static constexpr size_t length = 8; ///< フィールド位置。
            static constexpr size_t breakpoints = 12; ///< フィールド位置。
            static constexpr size_t points = 16; ///< フィールド位置。
            static constexpr size_t crc = 20; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct BundleHeader {
            uint32_t signature; ///< wire上の値。
            uint16_t version; ///< wire上の値。
            uint16_t flags; ///< wire上の値。
            uint32_t length; ///< wire上の値。
            uint32_t breakpoints; ///< wire上の値。
            uint32_t points; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 24バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        BundleHeader readBundleHeader(const uint8_t *data);

        /**
         * @brief DebugHeaderの既存wire配置。
         */
        struct DebugHeaderLayout {
            static constexpr size_t ByteLength = 8; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t revision = 4; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugHeader {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 8バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugHeader readDebugHeader(const uint8_t *data);

        /**
         * @brief DebugBeginの既存wire配置。
         */
        struct DebugBeginLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t revision = 4; ///< フィールド位置。
            static constexpr size_t count = 8; ///< フィールド位置。
            static constexpr size_t crc = 12; ///< フィールド位置。
            static constexpr size_t imageCrc = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugBegin {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t count; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
            uint32_t imageCrc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 20バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugBegin readDebugBegin(const uint8_t *data);

        /**
         * @brief DebugDataの既存wire配置。
         */
        struct DebugDataLayout {
            static constexpr size_t ByteLength = 12; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t revision = 4; ///< フィールド位置。
            static constexpr size_t index = 8; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugData {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t index; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 12バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugData readDebugData(const uint8_t *data);

        /**
         * @brief DebugChunkの既存wire配置。
         */
        struct DebugChunkLayout {
            static constexpr size_t ByteLength = 16; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t revision = 4; ///< フィールド位置。
            static constexpr size_t stopId = 8; ///< フィールド位置。
            static constexpr size_t offset = 12; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugChunk {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t stopId; ///< wire上の値。
            uint32_t offset; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 16バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugChunk readDebugChunk(const uint8_t *data);

        /**
         * @brief DebugExecuteの既存wire配置。
         */
        struct DebugExecuteLayout {
            static constexpr size_t ByteLength = 12; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t revision = 4; ///< フィールド位置。
            static constexpr size_t stopId = 8; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugExecute {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t stopId; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 12バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugExecute readDebugExecute(const uint8_t *data);

        /**
         * @brief DebugResultの既存wire配置。
         */
        struct DebugResultLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t request = 2; ///< フィールド位置。
            static constexpr size_t code = 4; ///< フィールド位置。
            static constexpr size_t state = 5; ///< フィールド位置。
            static constexpr size_t reserved = 6; ///< フィールド位置。
            static constexpr size_t revision = 8; ///< フィールド位置。
            static constexpr size_t value = 12; ///< フィールド位置。
            static constexpr size_t extra = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugResult {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t request; ///< wire上の値。
            uint8_t code; ///< wire上の値。
            uint8_t state; ///< wire上の値。
            uint16_t reserved; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t value; ///< wire上の値。
            uint32_t extra; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 20バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeDebugResult(uint8_t *out, const DebugResult &packet);

        /**
         * @brief DebugEventの既存wire配置。
         */
        struct DebugEventLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 2; ///< 宣言した既定値。
            static constexpr size_t state = 1; ///< フィールド位置。
            static constexpr size_t code = 2; ///< フィールド位置。
            static constexpr size_t error = 3; ///< フィールド位置。
            static constexpr size_t stopId = 4; ///< フィールド位置。
            static constexpr size_t revision = 8; ///< フィールド位置。
            static constexpr size_t size = 12; ///< フィールド位置。
            static constexpr size_t crc = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DebugEvent {
            uint8_t version; ///< wire上の値。
            uint8_t state; ///< wire上の値。
            uint8_t code; ///< wire上の値。
            uint8_t error; ///< wire上の値。
            uint32_t stopId; ///< wire上の値。
            uint32_t revision; ///< wire上の値。
            uint32_t size; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 20バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DebugEvent readDebugEvent(const uint8_t *data);
        /**
         * @brief 固定長の値を書き込む。
         * @param out 20バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeDebugEvent(uint8_t *out, const DebugEvent &packet);

        /**
         * @brief SnapshotHeaderの既存wire配置。
         */
        struct SnapshotHeaderLayout {
            static constexpr size_t ByteLength = 24; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 6; ///< 宣言した既定値。
            static constexpr size_t reason = 1; ///< フィールド位置。
            static constexpr size_t slotBytes = 2; ///< フィールド位置。
            static constexpr size_t pc = 4; ///< フィールド位置。
            static constexpr size_t bp = 8; ///< フィールド位置。
            static constexpr size_t sp = 12; ///< フィールド位置。
            static constexpr size_t globals = 16; ///< フィールド位置。
            static constexpr size_t stack = 20; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct SnapshotHeader {
            uint8_t version; ///< wire上の値。
            uint8_t reason; ///< wire上の値。
            uint16_t slotBytes; ///< wire上の値。
            uint32_t pc; ///< wire上の値。
            uint32_t bp; ///< wire上の値。
            uint32_t sp; ///< wire上の値。
            uint32_t globals; ///< wire上の値。
            uint32_t stack; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 24バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeSnapshotHeader(uint8_t *out, const SnapshotHeader &packet);

        /**
         * @brief SnapshotValueの既存wire配置。
         */
        struct SnapshotValueLayout {
            static constexpr size_t ByteLength = 8; ///< 固定領域の長さ。
            static constexpr size_t tag = 0; ///< フィールド位置。
            static constexpr size_t reserved = 1; ///< フィールド位置。
            static constexpr size_t classId = 2; ///< フィールド位置。
            static constexpr size_t payload = 4; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct SnapshotValue {
            uint8_t tag; ///< wire上の値。
            uint8_t reserved; ///< wire上の値。
            uint16_t classId; ///< wire上の値。
            uint32_t payload; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 8バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeSnapshotValue(uint8_t *out, const SnapshotValue &packet);

        /**
         * @brief StringHeaderの既存wire配置。
         */
        struct StringHeaderLayout {
            static constexpr size_t ByteLength = 8; ///< 固定領域の長さ。
            static constexpr size_t id = 0; ///< フィールド位置。
            static constexpr size_t length = 4; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct StringHeader {
            uint32_t id; ///< wire上の値。
            uint32_t length; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 8バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeStringHeader(uint8_t *out, const StringHeader &packet);

        /**
         * @brief Word32の既存wire配置。
         */
        struct Word32Layout {
            static constexpr size_t ByteLength = 4; ///< 固定領域の長さ。
            static constexpr size_t value = 0; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct Word32 {
            uint32_t value; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 4バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeWord32(uint8_t *out, const Word32 &packet);

        /**
         * @brief Pcの既存wire配置。
         */
        struct PcLayout {
            static constexpr size_t ByteLength = 2; ///< 固定領域の長さ。
            static constexpr size_t value = 0; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct Pc {
            uint16_t value; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 2バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        Pc readPc(const uint8_t *data);

        /**
         * @brief RemoteHeaderの既存wire配置。
         */
        struct RemoteHeaderLayout {
            static constexpr size_t ByteLength = 8; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t nonce = 4; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteHeader {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 8バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RemoteHeader readRemoteHeader(const uint8_t *data);

        /**
         * @brief RemoteBeginの既存wire配置。
         */
        struct RemoteBeginLayout {
            static constexpr size_t ByteLength = 14; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t nonce = 4; ///< フィールド位置。
            static constexpr size_t total = 8; ///< フィールド位置。
            static constexpr size_t crc = 10; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteBegin {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
            uint16_t total; ///< wire上の値。
            uint32_t crc; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 14バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RemoteBegin readRemoteBegin(const uint8_t *data);

        /**
         * @brief RemoteDataの既存wire配置。
         */
        struct RemoteDataLayout {
            static constexpr size_t ByteLength = 10; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t nonce = 4; ///< フィールド位置。
            static constexpr size_t offset = 8; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteData {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
            uint16_t offset; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 10バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RemoteData readRemoteData(const uint8_t *data);

        /**
         * @brief RemoteAckの既存wire配置。
         */
        struct RemoteAckLayout {
            static constexpr size_t ByteLength = 12; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t nonce = 4; ///< フィールド位置。
            static constexpr size_t id = 8; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteAck {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
            uint32_t id; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 12バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RemoteAck readRemoteAck(const uint8_t *data);

        /**
         * @brief RemoteResultの既存wire配置。
         */
        struct RemoteResultLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t op = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t nonce = 4; ///< フィールド位置。
            static constexpr size_t code = 8; ///< フィールド位置。
            static constexpr size_t state = 9; ///< フィールド位置。
            static constexpr size_t received = 10; ///< フィールド位置。
            static constexpr size_t reserved = 12; ///< フィールド位置。
            static constexpr size_t extra = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteResult {
            uint8_t version; ///< wire上の値。
            uint8_t op; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
            uint8_t code; ///< wire上の値。
            uint8_t state; ///< wire上の値。
            uint16_t received; ///< wire上の値。
            uint32_t reserved; ///< wire上の値。
            uint32_t extra; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 20バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeRemoteResult(uint8_t *out, const RemoteResult &packet);

        /**
         * @brief RemoteEventの既存wire配置。
         */
        struct RemoteEventLayout {
            static constexpr size_t ByteLength = 20; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t kind = 1; ///< フィールド位置。
            static constexpr size_t nonce = 2; ///< フィールド位置。
            static constexpr size_t id = 6; ///< フィールド位置。
            static constexpr size_t object = 10; ///< フィールド位置。
            static constexpr size_t method = 12; ///< フィールド位置。
            static constexpr size_t argc = 14; ///< フィールド位置。
            static constexpr size_t error = 15; ///< フィールド位置。
            static constexpr size_t value = 16; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RemoteEvent {
            uint8_t version; ///< wire上の値。
            uint8_t kind; ///< wire上の値。
            uint32_t nonce; ///< wire上の値。
            uint32_t id; ///< wire上の値。
            uint16_t object; ///< wire上の値。
            uint16_t method; ///< wire上の値。
            uint8_t argc; ///< wire上の値。
            uint8_t error; ///< wire上の値。
            int32_t value; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 20バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeRemoteEvent(uint8_t *out, const RemoteEvent &packet);

        /**
         * @brief RegistryHeaderの既存wire配置。
         */
        struct RegistryHeaderLayout {
            static constexpr size_t ByteLength = 100; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 1; ///< 宣言した既定値。
            static constexpr size_t count = 1; ///< フィールド位置。
            static constexpr size_t reserved = 2; ///< フィールド位置。
            static constexpr size_t baseHash = 4; ///< フィールド位置。
            static constexpr size_t rpcHash = 36; ///< フィールド位置。
            static constexpr size_t registryHash = 68; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RegistryHeader {
            uint8_t version; ///< wire上の値。
            uint8_t count; ///< wire上の値。
            uint16_t reserved; ///< wire上の値。
            const uint8_t *baseHash; ///< wire上の値。
            const uint8_t *rpcHash; ///< wire上の値。
            const uint8_t *registryHash; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 100バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RegistryHeader readRegistryHeader(const uint8_t *data);

        /**
         * @brief RegistryRecordの既存wire配置。
         */
        struct RegistryRecordLayout {
            static constexpr size_t ByteLength = 10; ///< 固定領域の長さ。
            static constexpr size_t native = 0; ///< フィールド位置。
            static constexpr size_t object = 2; ///< フィールド位置。
            static constexpr size_t method = 4; ///< フィールド位置。
            static constexpr size_t classId = 6; ///< フィールド位置。
            static constexpr size_t argc = 8; ///< フィールド位置。
            static constexpr size_t reserved = 9; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct RegistryRecord {
            uint16_t native; ///< wire上の値。
            uint16_t object; ///< wire上の値。
            uint16_t method; ///< wire上の値。
            uint16_t classId; ///< wire上の値。
            uint8_t argc; ///< wire上の値。
            uint8_t reserved; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 10バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        RegistryRecord readRegistryRecord(const uint8_t *data);

        /**
         * @brief ProvisionFrameの既存wire配置。
         */
        struct ProvisionFrameLayout {
            static constexpr size_t ByteLength = 1; ///< 固定領域の長さ。
            static constexpr size_t op = 0; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct ProvisionFrame {
            uint8_t op; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 1バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        ProvisionFrame readProvisionFrame(const uint8_t *data);

        /**
         * @brief DeviceTagの既存wire配置。
         */
        struct DeviceTagLayout {
            static constexpr size_t ByteLength = 1; ///< 固定領域の長さ。
            static constexpr size_t deviceType = 0; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DeviceTag {
            uint8_t deviceType; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 1バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DeviceTag readDeviceTag(const uint8_t *data);

        /**
         * @brief DeviceServoの既存wire配置。
         */
        struct DeviceServoLayout {
            static constexpr size_t ByteLength = 3; ///< 固定領域の長さ。
            static constexpr size_t deviceType = 0; ///< フィールド位置。
            static constexpr size_t channel = 1; ///< フィールド位置。
            static constexpr uint32_t channelMax = 15; ///< 宣言した範囲・予約値。
            static constexpr size_t mode = 2; ///< フィールド位置。
            static constexpr uint32_t modeMax = 1; ///< 宣言した範囲・予約値。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DeviceServo {
            uint8_t deviceType; ///< wire上の値。
            uint8_t channel; ///< wire上の値。
            uint8_t mode; ///< wire上の値。
        };
        /**
         * @brief 検査済み領域を復号する。
         * @param data 3バイト以上の領域。
         * @return bytesはdataと同じ寿命の借用。
         */
        DeviceServo readDeviceServo(const uint8_t *data);

        /**
         * @brief TelemetryHeaderの既存wire配置。
         */
        struct TelemetryHeaderLayout {
            static constexpr size_t ByteLength = 8; ///< 固定領域の長さ。
            static constexpr size_t version = 0; ///< フィールド位置。
            static constexpr uint32_t versionDefault = 3; ///< 宣言した既定値。
            static constexpr size_t flags = 1; ///< フィールド位置。
            static constexpr size_t sequence = 2; ///< フィールド位置。
            static constexpr size_t millis = 4; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct TelemetryHeader {
            uint8_t version; ///< wire上の値。
            uint8_t flags; ///< wire上の値。
            uint16_t sequence; ///< wire上の値。
            uint32_t millis; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 8バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeTelemetryHeader(uint8_t *out, const TelemetryHeader &packet);

        /**
         * @brief ImuRecordの既存wire配置。
         */
        struct ImuRecordLayout {
            static constexpr size_t ByteLength = 13; ///< 固定領域の長さ。
            static constexpr size_t deviceType = 0; ///< フィールド位置。
            static constexpr size_t accX = 1; ///< フィールド位置。
            static constexpr size_t accY = 3; ///< フィールド位置。
            static constexpr size_t accZ = 5; ///< フィールド位置。
            static constexpr size_t gyroX = 7; ///< フィールド位置。
            static constexpr size_t gyroY = 9; ///< フィールド位置。
            static constexpr size_t gyroZ = 11; ///< フィールド位置。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct ImuRecord {
            uint8_t deviceType; ///< wire上の値。
            int16_t accX; ///< wire上の値。
            int16_t accY; ///< wire上の値。
            int16_t accZ; ///< wire上の値。
            int16_t gyroX; ///< wire上の値。
            int16_t gyroY; ///< wire上の値。
            int16_t gyroZ; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 13バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeImuRecord(uint8_t *out, const ImuRecord &packet);

        /**
         * @brief DistanceRecordの既存wire配置。
         */
        struct DistanceRecordLayout {
            static constexpr size_t ByteLength = 3; ///< 固定領域の長さ。
            static constexpr size_t deviceType = 0; ///< フィールド位置。
            static constexpr size_t distanceMm = 1; ///< フィールド位置。
            static constexpr uint32_t distanceMmMin = 20; ///< 宣言した範囲・予約値。
            static constexpr uint32_t distanceMmMax = 4000; ///< 宣言した範囲・予約値。
            static constexpr uint32_t distanceMmSentinel = 65535; ///< 宣言した範囲・予約値。
        };
        /**
         * @brief 復号値。bytesは受信バッファーを借用し、直接構造体を送らない。
         */
        struct DistanceRecord {
            uint8_t deviceType; ///< wire上の値。
            uint16_t distanceMm; ///< wire上の値。
        };
        /**
         * @brief 固定長の値を書き込む。
         * @param out 3バイト以上の出力領域。
         * @param packet 幅とbytesの寿命を保証した値。
         */
        void writeDistanceRecord(uint8_t *out, const DistanceRecord &packet);

    }
}
namespace uroa {
    namespace wire {
        /**
         * @brief 生成元の固定値・範囲・予約値を検査する。
         * @param packet 復号した値。
         * @return wire上の値が有効ならtrue。
         */
        bool validDeviceServo(const DeviceServo &packet);
        /**
         * @brief 生成元の固定値・範囲・予約値を検査する。
         * @param packet 復号した値。
         * @return wire上の値が有効ならtrue。
         */
        bool validTelemetryHeader(const TelemetryHeader &packet);
        /**
         * @brief 生成元の固定値・範囲・予約値を検査する。
         * @param packet 復号した値。
         * @return wire上の値が有効ならtrue。
         */
        bool validDistanceRecord(const DistanceRecord &packet);
        namespace DeviceInfoRecordsLimits {
            constexpr size_t MaxRecords = 20; ///< 教材の登録件数上限。
            constexpr size_t MaxBytes = 20; ///< 教材の列全体の上限。
        }
        /**
         * @brief 種別からレコード長を求める。
         * @param kind 種別。
         * @return 不明な種別なら0。
         */
        size_t DeviceInfoRecordsRecordLength(uint8_t kind);
        namespace TelemetryRecordsLimits {
            constexpr size_t MaxRecords = 20; ///< 教材の登録件数上限。
            constexpr size_t MaxBytes = 268; ///< 教材の列全体の上限。
        }
        /**
         * @brief 種別からレコード長を求める。
         * @param kind 種別。
         * @return 不明な種別なら0。
         */
        size_t TelemetryRecordsRecordLength(uint8_t kind);
        /**
         * @brief 同種別の物理サンプルを共有する列の値。呼出し中だけ借用する。
         */
        struct TelemetryRecordsValues {
            const ImuRecord &imuRecord; ///< 計測済みの値。
            const DistanceRecord &distanceRecord; ///< 計測済みの値。
        };
        /**
         * @brief 種別列から必要長を求める。
         * @param kinds 種別列。
         * @param count 件数。
         * @return 不明・オーバーフローなら0。
         */
        size_t TelemetryRecordsLength(const uint8_t *kinds, size_t count);
        /**
         * @brief 全件検証後にヘッダーとレコード列を書く。
         * @param out 出力。
         * @param capacity 容量。
         * @param header ヘッダー。
         * @param kinds 登録順の種別。
         * @param count 件数。
         * @param values 計測済みの値。
         * @return 不正なら0で出力を変えない。
         */
        size_t writeTelemetryRecords(uint8_t *out, size_t capacity, const TelemetryHeader &header, const uint8_t *kinds, size_t count, const TelemetryRecordsValues &values);
    }
}
