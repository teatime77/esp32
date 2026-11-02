/**
 * @file uroa_ultrasonic.cpp
 * @brief HC-SR04のトリガー生成、ECHO幅の取得と距離変換を実装する。
 */
#include "uroa_ultrasonic.h"
#include <driver/gpio.h>

namespace uroa {
    /**
     * @brief 教材側のGPIO設定をコピーして保持する。
     * @param config TRIGとECHOのGPIO設定。
     */
    HcSr04::HcSr04(const Config &config) : config_(config) {
    }

    /**
     * @brief GPIOを検査し、TRIGとECHOを初期化する。
     * @return 異なる有効GPIOを設定できたらtrue。センサーの応答は検査しない。
     */
    bool HcSr04::begin() {
        end();
        // ESP-IDFのGPIO判定マクロ内で不正なビットシフトをしないよう、先に範囲を調べる。
        if (config_.triggerPin < 0 || config_.triggerPin >= GPIO_NUM_MAX || config_.echoPin < 0 ||
            config_.echoPin >= GPIO_NUM_MAX) {
            return false;
        }
        if (!GPIO_IS_VALID_OUTPUT_GPIO(config_.triggerPin) || !GPIO_IS_VALID_GPIO(config_.echoPin) ||
            config_.triggerPin == config_.echoPin) {
            return false;
        }

        pinMode(config_.triggerPin, OUTPUT);
        digitalWrite(config_.triggerPin, LOW);
        pinMode(config_.echoPin, INPUT);
        lastAttemptMs_ = millis();
        initialized = true;
        return true;
    }

    /**
     * @brief 測定間隔を検査し、ECHO幅から2〜400cmの距離を求める。
     * @return 測定結果。Okの場合だけdistanceCmに新しい有効値を設定する。
     */
    HcSr04::Result HcSr04::read() {
        if (!initialized) {
            return Result::NotInitialized;
        }

        const uint32_t now = millis();
        if (static_cast<uint32_t>(now - lastAttemptMs_) < 65) {
            return Result::TooSoon;
        }

        lastAttemptMs_ = now;
        distanceCm = NAN;
        echoDurationUs = 0;
        if (digitalRead(config_.echoPin) != LOW) {
            return Result::EchoHigh;
        }

        digitalWrite(config_.triggerPin, LOW);
        delayMicroseconds(2);
        digitalWrite(config_.triggerPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(config_.triggerPin, LOW);

        // 無反射やECHOの固着でも待機を打ち切る。割り込みは無効化しない。
        echoDurationUs = pulseIn(config_.echoPin, HIGH, 30000UL);
        if (echoDurationUs == 0) {
            return Result::Timeout;
        }

        // データシートの式（us / 58 = cm）を使用する。温度補償は行わない。
        const float measuredCm = echoDurationUs / 58.0f;
        if (measuredCm < 2.0f || measuredCm > 400.0f) {
            return Result::OutOfRange;
        }

        distanceCm = measuredCm;
        return Result::Ok;
    }

    /** @brief TRIG出力を解除し、初期化状態と測定値を消去する。 */
    void HcSr04::end() {
        if (initialized) {
            digitalWrite(config_.triggerPin, LOW);
            pinMode(config_.triggerPin, INPUT);
        }
        initialized = false;
        distanceCm = NAN;
        echoDurationUs = 0;
        lastAttemptMs_ = 0;
    }
}
