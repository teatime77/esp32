/**
 * @file uroa_servo.cpp
 * @brief PCA9685の角度サーボ出力、有効化、補間と停止を管理する。
 */
#include "uroa_servo.h"
#include <math.h>

namespace uroa {
    /**
     * @brief I2Cバスと教材のサーボ設定を保持する。
     * @param wire 教材側で開始するI2Cバス。
     * @param config 教材側から渡す設定。
     */
    Pca9685Servos::Pca9685Servos(TwoWire &wire, const Config &config)
        : wire_(wire), config_(config), pwm_(config.address, wire) {
    }

    /**
     * @brief PCA9685を初期化し、全CHをOFFにしてPWM周波数を設定する。
     * @return 初期化と出力停止が成功したらtrue。自動ARMは行わない。
     */
    bool Pca9685Servos::begin() {
        wire_.beginTransmission(config_.address);
        available = wire_.endTransmission() == 0 && pwm_.begin();
        initialized_ = available;
        if (available) {
            allOff();
            pwm_.setPWMFreq(config_.frequency);
            allOff();
        }
        return available;
    }

    /**
     * @brief 有効化中のCHがあるか調べる。
     * @return 1つでも有効ならtrue。
     */
    bool Pca9685Servos::anyArmed() const {
        for (const auto &s : states) {
            if (s.armed) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 補間が完了するまでの残り時間を計算する。
     * @param ch 出力CH番号（0〜15）。
     * @param now 呼出元の現在時刻（ms、周回あり）。
     * @return 残り時間（ms）。補間中でなければ0。
     */
    uint16_t Pca9685Servos::remaining(uint8_t ch, uint32_t now) const {
        if (ch >= 16) {
            return 0;
        }
        const auto *s = &states[ch];
        const uint32_t elapsed = now - s->started; // millis()の桁あふれにも対応
        return s->moving && elapsed < s->duration ? s->duration - elapsed : 0;
    }

    /**
     * @brief 全CHを無効化し、初期化済みPCAへ停止を書き込む。
     */
    void Pca9685Servos::allOff() {
        for (uint8_t ch = 0; ch < 16; ++ch) {
            states[ch].armed = states[ch].moving = false;
            states[ch].speed = 0;
            // 異常検出後も、初期化できたPCAへのOFF書き込みを全CHで試みる。
            if (initialized_ && pwm_.setPWM(ch, 0, 4096) != 0) {
                available = false;
            }
        }
    }

    /**
     * @brief CH、利用可否、角度と移動時間を検査する。
     * @param ch 出力CH番号（0〜15）。
     * @param angle 角度（0.01度）。
     * @param ms 移動時間（ms）。0は即時出力。
     * @return 最初に見つかったエラー、またはOk。
     */
    Pca9685Servos::Result Pca9685Servos::validate(uint8_t ch, uint16_t angle, uint16_t ms) const {
        if (ch >= 16) {
            return Result::Range;
        }
        if (!available) {
            return Result::Unavailable;
        }
        if (angle < config_.minAngle || angle > config_.maxAngle || ms > 10000) {
            return Result::Range;
        }
        return Result::Ok;
    }

    /**
     * @brief 角度をPWMパルスへ変換して出力する。失敗時は全CHの停止を試みる。
     * @param ch 出力CH番号（0〜15）。
     * @param angle 角度（0.01度）。
     * @return PWMの書き込みに成功したらtrue。
     */
    bool Pca9685Servos::output(uint8_t ch, uint16_t angle) {
        const float us = config_.minPulseUs + (config_.maxPulseUs - config_.minPulseUs) * (angle / 18000.0f);
        return outputPulse(ch, us);
    }

    /** @brief パルス幅を出力し、失敗時は全CHを停止する。@param ch CH。@param microseconds µs。@return 成功ならtrue。 */
    bool Pca9685Servos::outputPulse(uint8_t ch, float microseconds) {
        const float ticks = microseconds * config_.frequency * 4096.0f / 1000000.0f;
        if (!isfinite(ticks) || ticks < 1 || ticks > 4095) {
            available = false;
            allOff();
            return false;
        }
        if (pwm_.setPWM(ch, 0, lroundf(ticks)) == 0) {
            return true;
        }
        available = false;
        allOff();
        return false; // 応答の送信は教材側で行い、機器クラスを通信仕様から独立させる。
    }

    /**
     * @brief 指定CHを有効化し、直ちに角度指令を出力する。
     * @param ch 出力CH番号（0〜15）。
     * @param angle 角度（0.01度）。
     * @param ms 移動時間（ms）。0は即時出力。
     * @return 範囲検査と出力の結果。
     */
    Pca9685Servos::Result Pca9685Servos::arm(uint8_t ch, uint16_t angle, uint16_t ms) {
        const auto result = validate(ch, angle, ms);
        if (result != Result::Ok) {
            return result;
        }
        auto &s = states[ch];
        if (s.armed && s.mode != Mode::Angle) {
            return Result::WrongMode;
        }
        s.mode = Mode::Angle;
        s.armed = true;
        s.moving = false;
        s.current = s.start = s.target = angle;
        return output(ch, angle) ? Result::Ok : Result::IoError;
    }

    /**
     * @brief 有効なCHの角度を補間する。時間0なら直ちに出力する。
     * @param ch 出力CH番号（0〜15）。
     * @param degrees 角度（度）。
     * @param ms 移動時間（ms）。0は即時出力。
     * @param now 呼出元の現在時刻（ms、周回あり）。
     * @return 範囲検査、有効状態と出力の結果。
     */
    Pca9685Servos::Result Pca9685Servos::moveAngle(uint8_t ch, float degrees, uint16_t ms, uint32_t now) {
        if (!isfinite(degrees) || degrees < 0 || degrees > 180 || degrees < config_.minAngle / 100.0f ||
            degrees > config_.maxAngle / 100.0f) {
            return Result::Range;
        }
        const uint16_t angle = lroundf(degrees * 100);
        const auto result = validate(ch, angle, ms);
        if (result != Result::Ok) {
            return result;
        }
        auto &s = states[ch];
        if (!s.armed) {
            return Result::NotArmed;
        }
        if (s.mode != Mode::Angle) {
            return Result::WrongMode;
        }
        s.start = s.current;
        s.target = angle;
        s.duration = ms;
        s.started = now;
        s.moving = ms > 0;
        if (ms == 0) {
            s.current = angle;
            if (!output(ch, angle)) {
                return Result::IoError;
            }
        }
        return Result::Ok;
    }

    /**
     * @brief 指定CHの補間を止め、PWMを無効にする。
     * @param ch 出力CH番号（0〜15）。
     * @return 停止処理の結果。
     */
    Pca9685Servos::Result Pca9685Servos::disable(uint8_t ch) {
        if (ch >= 16) {
            return Result::Range;
        }
        states[ch].armed = states[ch].moving = false;
        states[ch].speed = 0;
        if (initialized_ && pwm_.setPWM(ch, 0, 4096) != 0) {
            available = false;
            allOff();
            return Result::IoError;
        }
        return Result::Ok;
    }

    /**
     * @brief 20ms以上の間隔で全CHの補間と出力を進める。
     * @param now 呼出元の現在時刻（ms、周回あり）。
     * @return 出力に失敗したCH。正常または更新不要なら-1。
     */
    int Pca9685Servos::tick(uint32_t now) {
        if (now - lastMotion_ < 20) {
            return -1;
        }
        lastMotion_ = now;
        for (uint8_t ch = 0; ch < 16; ++ch) {
            auto &s = states[ch];
            if (!s.armed || !s.moving || s.mode != Mode::Angle) {
                continue;
            }
            const uint32_t elapsed = now - s.started;
            if (elapsed >= s.duration) {
                s.current = s.target;
                s.moving = false;
            }
            else {
                s.current = int32_t(s.start) + (int32_t(s.target) - s.start) * int32_t(elapsed) / s.duration;
            }
            if (!output(ch, s.current)) {
                return ch;
            }
        }
        return -1;
    }

    /** @brief 度を旧ARM単位へ換算して有効化する。@param ch CH。@param degrees 度。@return 操作結果。 */
    Pca9685Servos::Result Pca9685Servos::armAngle(uint8_t ch, float degrees) {
        if (!isfinite(degrees) || degrees < 0 || degrees > 180 || degrees < config_.minAngle / 100.0f ||
            degrees > config_.maxAngle / 100.0f) {
            return Result::Range;
        }
        return arm(ch, lroundf(degrees * 100));
    }

    /** @brief 旧0.01度APIを度へ換算して移動する。@param ch CH。@param angle 0.01度。@param duration ms。@param now 現在時刻ms。@return 操作結果。 */
    Pca9685Servos::Result Pca9685Servos::move(uint8_t ch, uint16_t angle, uint16_t duration, uint32_t now) {
        return moveAngle(ch, angle / 100.0f, duration, now);
    }

    /** @brief 連続回転サーボの校正値を検査する。@param ch CH。@return 有効ならtrue。 */
    bool Pca9685Servos::validSpeedConfig(uint8_t ch) const {
        const auto &config = config_.speed[ch];
        return config.spanUs > 0 && int32_t(config.neutralUs) - config.spanUs >= 700 &&
            int32_t(config.neutralUs) + config.spanUs <= 2300 && (config.direction == 1 || config.direction == -1);
    }

    /** @brief 速度0%で有効化する。@param ch CH0〜15。@return 検査・出力結果。 */
    Pca9685Servos::Result Pca9685Servos::armSpeed(uint8_t ch) {
        if (ch >= 16) {
            return Result::Range;
        }
        if (!available) {
            return Result::Unavailable;
        }
        if (!validSpeedConfig(ch)) {
            return Result::Range;
        }
        auto &state = states[ch];
        if (state.armed && state.mode != Mode::Speed) {
            return Result::WrongMode;
        }
        state.mode = Mode::Speed;
        state.armed = true;
        state.moving = false;
        state.speed = 0;
        return outputPulse(ch, config_.speed[ch].neutralUs) ? Result::Ok : Result::IoError;
    }

    /** @brief 速度指令を即時に反映する。@param ch CH0〜15。@param percent −100〜100%。@return 検査・出力結果。 */
    Pca9685Servos::Result Pca9685Servos::setSpeed(uint8_t ch, float percent) {
        if (ch >= 16) {
            return Result::Range;
        }
        if (!available) {
            return Result::Unavailable;
        }
        if (!isfinite(percent) || percent < -100 || percent > 100 || !validSpeedConfig(ch)) {
            return Result::Range;
        }
        auto &state = states[ch];
        if (!state.armed) {
            return Result::NotArmed;
        }
        if (state.mode != Mode::Speed) {
            return Result::WrongMode;
        }
        const auto &config = config_.speed[ch];
        const float speed = lroundf(percent * 100) / 100.0f;
        const float microseconds = config.neutralUs + config.direction * speed * config.spanUs / 100;
        if (!outputPulse(ch, microseconds)) {
            return Result::IoError;
        }
        state.speed = speed;
        return Result::Ok;
    }

}
