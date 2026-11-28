/**
 * @file config.h
 * @brief diagram-controlのピン、サーボ範囲、通信UUIDと教材用の公開設定。
 * @details UUIDと教材用共通値はuroaのdiagramに合わせる。実際のWi-Fi認証情報は書かない。
 */
#pragma once
#include <uroa_wire.generated.h>
#include <stdint.h>

#if defined(UROA_BOARD_SUPERMINI_ESP32S3)
constexpr char DEVICE_NAME_PREFIX[] = "ESP32S3-Mini"; ///< 表示名の機種部分。固有番号の付加はdevice_name.cppが担当する。
constexpr int SDA_PIN = 8; ///< ESP32-S3 SuperMini 教材の共有I2CバスのSDA GPIO。基板ごとの分岐で指定する。
constexpr int SCL_PIN = 9; ///< 教材の共有I2CバスのSCL GPIO。基板ごとの分岐で指定する。
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1; ///< HC-SR04のECHO GPIO。-1は未割り当てで測定タスクを開始しない。
#elif defined(ARDUINO_XIAO_ESP32S3)
constexpr char DEVICE_NAME_PREFIX[] = "XIAO-ESP32S3"; ///< BLEとWi-Fiで共有する機種名部分。makeDeviceName()が固有番号を付加する。
constexpr int SDA_PIN = 5; ///< XIAO D4 教材の共有I2CバスのSDA GPIO。基板ごとの分岐で指定する。
constexpr int SCL_PIN = 6; ///< XIAO D5 教材の共有I2CバスのSCL GPIO。基板ごとの分岐で指定する。
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1; ///< HC-SR04のECHO GPIO。-1は未割り当てで測定タスクを開始しない。
#else
constexpr char DEVICE_NAME_PREFIX[] = "ESP32-DevKit"; ///< BLEとWi-Fiで共有する機種名部分。makeDeviceName()が固有番号を付加する。
constexpr int SDA_PIN = 21; ///< ESP32 DevKit V1 教材の共有I2CバスのSDA GPIO。基板ごとの分岐で指定する。
constexpr int SCL_PIN = 22; ///< 教材の共有I2CバスのSCL GPIO。基板ごとの分岐で指定する。
// DOIT ESP32 DEVKIT V1: I2C、UART0、Flash、起動設定ピンとの重複を避ける。
constexpr int ULTRASONIC_TRIGGER_PIN = 25; ///< HC-SR04のTRIGへ接続するGPIO出力。
constexpr int ULTRASONIC_ECHO_PIN = 26; ///< 分圧またはレベル変換したECHOを受けるGPIO入力。
#endif
constexpr uint32_t ULTRASONIC_SAMPLE_INTERVAL_MS = 100; ///< 超音波測定タスクが各処理後に待つ時間（ms）。
constexpr uint8_t DISPLAY_ADDRESS = 0x3C; ///< PythonのDisplay()が使うSSD1306のI2Cアドレス。
constexpr uint8_t DISPLAY_WIDTH = 128; ///< SSD1306表示幅。PAGE転送用に高さは8の倍数とする。
constexpr uint8_t DISPLAY_HEIGHT = 64; ///< SSD1306の表示高さ（pixel）。ページ転送のため8の倍数とする。
constexpr uint8_t PCA_ADDRESS = 0x40; ///< 共有I2Cバス上のPCA9685の7bitアドレス。
constexpr float SERVO_FREQUENCY = 50.0f; ///< PCA9685へ設定するサーボPWM周波数（Hz）。
/**
 * @brief diagram-controlの角度サーボは全CH共通で0〜180度を受け付ける。
 * @details
 * 角度の単位は0.01度。main.cppのservoConfig()が共有ライブラリーの既定範囲を上書きし、
 * lib/servoの範囲検査をPythonのdevice_native.cppとBLE/Wi-Fi指令の両方へ適用する。
 * Web側のdevice-connection.tsとhardware/servo.tsも同じ受付範囲を使う。
 * 90度での明示的な有効化を維持し、範囲外の角度は拒否する。
 * lib/servoは0度をMIN_PULSE_US、180度をMAX_PULSE_USへ線形換算する。
 * SG90用の初期校正値は500〜2500µsとし、90度は1500µsになる。全CHの角度モードへ適用する。
 * KeyestudioのSG90仕様（約180度、500〜2500µs）を初期値の根拠とする。
 * https://www.keyestudio.com/products/keyestudio-micro-servo-sg90-0-180-degrees-for-arduino-smart-car-robot-aircraft-ce-certification
 * この値は個体ごとの校正結果ではない。機械的な端点へ当たる場合は、実測に基づいて幅を狭める。
 * この範囲は指令値であり、個体ごとの実際の回転角や機械的な端点を保証しない。
 */
constexpr uint16_t MIN_ANGLE = 0; ///< 全CH共通の角度指令の最小値。単位は0.01度。
constexpr uint16_t MAX_ANGLE = 18000; ///< 全CH共通の角度指令の最大値。単位は0.01度。
constexpr uint16_t MIN_PULSE_US = 500; ///< 0度指令に対応するパルス幅（µs）。
constexpr uint16_t MAX_PULSE_US = 2500; ///< 180度指令に対応するパルス幅（µs）。
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 5000; ///< 有効な指令が途絶えたとき、loop()が出力と通知を停止する期限（ms）。
constexpr const char *SERVICE_UUID = uroa::wire::GattUuidValues::service; ///< diagram-controlのBLE制御サービスのUUID。
constexpr const char *COMMAND_UUID = uroa::wire::GattUuidValues::command; ///< 通常指令を受け付けるGATT特性のUUID。
constexpr const char *TELEMETRY_UUID = uroa::wire::GattUuidValues::telemetry; ///< センサー通知を送るGATT特性のUUID。
constexpr const char *STATUS_UUID = uroa::wire::GattUuidValues::status; ///< 通常指令の受付結果をRead/Notifyで公開する特性のUUID。
constexpr const char *CONFIG_UUID = uroa::wire::GattUuidValues::wifiConfig; ///< Wi-Fi設定要求を受け付けるGATT特性のUUID。
constexpr const char *RESULT_UUID = uroa::wire::GattUuidValues::wifiResult; ///< Wi-Fi設定の処理結果を公開するGATT特性のUUID。
constexpr const char *DEVICE_INFO_UUID = uroa::wire::GattUuidValues::deviceInfo; ///< 機器情報の書込窓口。
constexpr const char *BYTECODE_UUID = uroa::wire::GattUuidValues::bytecode; ///< バイトコードの分割受信窓口。
constexpr const char *BYTECODE_RESULT_UUID = uroa::wire::GattUuidValues::bytecodeResult; ///< バイトコード受付結果のRead窓口。
constexpr const char *DEBUG_CONTROL_UUID = uroa::wire::GattUuidValues::debugControl; ///< デバッグ要求のWrite窓口。
constexpr const char *DEBUG_RESULT_UUID = uroa::wire::GattUuidValues::debugResult; ///< デバッグACKのRead窓口。
constexpr const char *DEBUG_EVENT_UUID = uroa::wire::GattUuidValues::debugEvent; ///< VM状態のRead/Notify窓口。
constexpr const char *DEBUG_DATA_UUID = uroa::wire::GattUuidValues::debugData; ///< 停止値チャンクのRead窓口。
constexpr uint32_t PAIRING_CODE = 123456; ///< diagramと共通の教材用BLEペアリングコード。公開設定として扱う。
// 公開の教材用共通値。robot-wifi.tsのteachingConnectionKeyと揃える。
constexpr char TEACHING_CONNECTION_KEY[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"; ///< Web側のteachingConnectionKeyと合わせる公開の教材用接続キー。
