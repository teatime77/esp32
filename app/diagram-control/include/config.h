/**
 * @file config.h
 * @brief diagram-controlのピン、サーボ範囲、通信UUIDと教材用の公開設定。
 * @details UUIDと教材用共通値はuroaのdiagramに合わせる。実際のWi-Fi認証情報は書かない。
 */
#pragma once
#include <stdint.h>

#if defined(UROA_BOARD_SUPERMINI_ESP32S3)
constexpr int SDA_PIN = 8; // ESP32-S3 SuperMini
constexpr int SCL_PIN = 9;
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1;
#elif defined(ARDUINO_XIAO_ESP32S3)
constexpr int SDA_PIN = 5; // XIAO D4
constexpr int SCL_PIN = 6; // XIAO D5
constexpr int ULTRASONIC_TRIGGER_PIN = -1; ///< この基板ではHC-SR04のピンを未割り当てとする。
constexpr int ULTRASONIC_ECHO_PIN = -1;
#else
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
// 角度サーボの初期範囲。機構と個体を確認してから調整する。
constexpr uint16_t MIN_ANGLE = 6000; // 0.01度
constexpr uint16_t MAX_ANGLE = 12000;
constexpr uint16_t MIN_PULSE_US = 1000; // 公称0度のパルス幅（µs）
constexpr uint16_t MAX_PULSE_US = 2000; // 公称180度のパルス幅（µs）
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 5000;
constexpr char DEVICE_NAME[] = "ESP32-Diagram-Control";
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
