/**
 * @file config.h
 * @brief diagram-controlのピン、サーボ範囲、通信UUIDと教材用の公開設定。
 * @details UUIDと教材用共通値はuroaのdiagramに合わせる。実際のWi-Fi認証情報は書かない。
 */
#pragma once
#include <stdint.h>

#if defined(UROA_BOARD_SUPERMINI_ESP32S3)
constexpr char DEVICE_NAME_PREFIX[] = "ESP32S3-Mini"; ///< 表示名の機種部分。固有番号の付加はdevice_name.cppが担当する。
constexpr int SDA_PIN = 8; // ESP32-S3 SuperMini
constexpr int SCL_PIN = 9;
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1;
#elif defined(ARDUINO_XIAO_ESP32S3)
constexpr char DEVICE_NAME_PREFIX[] = "XIAO-ESP32S3";
constexpr int SDA_PIN = 5; // XIAO D4
constexpr int SCL_PIN = 6; // XIAO D5
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1;
#else
constexpr char DEVICE_NAME_PREFIX[] = "ESP32-DevKit";
constexpr int SDA_PIN = 21; // ESP32 DevKit V1
constexpr int SCL_PIN = 22;
// DOIT ESP32 DEVKIT V1: I2C、UART0、Flash、起動設定ピンとの重複を避ける。
constexpr int ULTRASONIC_TRIGGER_PIN = 25; ///< HC-SR04のTRIGへ接続するGPIO出力。
constexpr int ULTRASONIC_ECHO_PIN = 26; ///< 分圧またはレベル変換したECHOを受けるGPIO入力。
#endif
constexpr uint32_t ULTRASONIC_SAMPLE_INTERVAL_MS = 100; ///< 超音波測定タスクが各処理後に待つ時間（ms）。
constexpr uint8_t DISPLAY_ADDRESS = 0x3C; ///< PythonのDisplay()が使うSSD1306のI2Cアドレス。
constexpr uint8_t DISPLAY_WIDTH = 128; ///< SSD1306表示幅。PAGE転送用に高さは8の倍数とする。
constexpr uint8_t DISPLAY_HEIGHT = 64;
constexpr uint8_t PCA_ADDRESS = 0x40;
constexpr float SERVO_FREQUENCY = 50.0f;
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
constexpr uint16_t MIN_ANGLE = 0;
constexpr uint16_t MAX_ANGLE = 18000;
constexpr uint16_t MIN_PULSE_US = 500; ///< 0度指令に対応するパルス幅（µs）。
constexpr uint16_t MAX_PULSE_US = 2500; ///< 180度指令に対応するパルス幅（µs）。
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 5000;
constexpr char SERVICE_UUID[] = "f3641400-00b0-4240-ba50-05ca45bf8abc";
constexpr char COMMAND_UUID[] = "f3641401-00b0-4240-ba50-05ca45bf8abc";
constexpr char TELEMETRY_UUID[] = "f3641402-00b0-4240-ba50-05ca45bf8abc";
constexpr char STATUS_UUID[] = "f3641403-00b0-4240-ba50-05ca45bf8abc";
constexpr char CONFIG_UUID[] = "f3641404-00b0-4240-ba50-05ca45bf8abc";
constexpr char RESULT_UUID[] = "f3641405-00b0-4240-ba50-05ca45bf8abc";
constexpr char DEVICE_INFO_UUID[] = "f3641406-00b0-4240-ba50-05ca45bf8abc"; ///< 機器情報の書込窓口。
constexpr char BYTECODE_UUID[] = "f3641407-00b0-4240-ba50-05ca45bf8abc"; ///< バイトコードの分割受信窓口。
constexpr char BYTECODE_RESULT_UUID[] = "f3641408-00b0-4240-ba50-05ca45bf8abc"; ///< バイトコード受付結果のRead窓口。
constexpr char DEBUG_CONTROL_UUID[] = "f3641409-00b0-4240-ba50-05ca45bf8abc"; ///< デバッグ要求のWrite窓口。
constexpr char DEBUG_RESULT_UUID[] = "f364140a-00b0-4240-ba50-05ca45bf8abc"; ///< デバッグACKのRead窓口。
constexpr char DEBUG_EVENT_UUID[] = "f364140b-00b0-4240-ba50-05ca45bf8abc"; ///< VM状態のRead/Notify窓口。
constexpr char DEBUG_DATA_UUID[] = "f364140c-00b0-4240-ba50-05ca45bf8abc"; ///< 停止値チャンクのRead窓口。
constexpr uint32_t PAIRING_CODE = 123456;
// 公開の教材用共通値。robot-wifi.tsのteachingConnectionKeyと揃える。
constexpr char TEACHING_CONNECTION_KEY[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
