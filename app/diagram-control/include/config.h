/**
 * @file config.h
 * @brief diagram-controlのピン、サーボ範囲、通信UUIDと教材用の公開設定。
 * @details UUIDと教材用共通値はuroaのdiagramに合わせる。実際のWi-Fi認証情報は書かない。
 */
#pragma once
#include <stdint.h>

#if defined(ARDUINO_XIAO_ESP32S3)
constexpr int SDA_PIN = 5; // XIAO D4
constexpr int SCL_PIN = 6; // XIAO D5
#else
constexpr int SDA_PIN = 21; // ESP32 DevKit V1
constexpr int SCL_PIN = 22;
#endif
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
constexpr uint32_t PAIRING_CODE = 123456;
// 公開の教材用共通値。robot-wifi.tsのteachingConnectionKeyと揃える。
constexpr char TEACHING_CONNECTION_KEY[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
