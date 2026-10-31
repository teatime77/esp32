/**
 * @file uroa_servo.h
 * @brief PCA9685の角度・速度指令、有効化、補間と停止を管理する。
 */
#pragma once
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

namespace uroa {
    /**
     * @brief 角度・連続回転サーボを同じPCA9685で制御する16CH。I2Cは教材が開始し、同じ制御タスクから操作する。
     * @details PCA9685の16CHを管理する。I2Cの開始・周期呼び出しは教材側が担当する。 BLE/HTTPコールバックからは呼ばず、同じ制御タスクから操作する。
     */
    class Pca9685Servos {
        public:
        /** @brief 指令の種類。変更前にCHを無効化し、改めて有効化する。 */
        enum class Mode {
            Angle,
            Speed
        };

        /** @brief 連続回転サーボの中立、変化幅（µs）と取り付け方向。 */
        struct SpeedConfig {
            uint16_t neutralUs = 1500;
            uint16_t spanUs = 500;
            int8_t direction = 1;
        };

        bool available = false; ///< 初期化・直近のI2C処理の結果。アプリは参照だけを行う。

        /**
         * @brief I2Cアドレス、PWM周波数、許可角度とパルス幅の設定。
         */
        struct Config {
            uint8_t address = 0x40;
            float frequency = 50.0f;
            uint16_t minAngle = 6000;
            uint16_t maxAngle = 12000; // 0.01度
            uint16_t minPulseUs = 1000;
            uint16_t maxPulseUs = 2000; // 公称0度／180度の幅
            SpeedConfig speed[16]; ///< CHごとの校正値。コンストラクターへ渡す前に設定する。
        };

        /**
         * @brief 各CHの有効状態、角度指令と補間時間。角度は実測値ではない。
         */
        struct State {
            Mode mode = Mode::Angle;
            float speed = 0; ///< 方向付き速度指令（%、−100〜100）。実測値ではない。
            bool armed = false;
            bool moving = false;
            uint16_t current = 9000;
            uint16_t start = 9000;
            uint16_t target = 9000;
            uint16_t duration = 0;
            uint32_t started = 0;
        };

        State states[16]; ///< CH0〜15の指令状態。実測角度ではなく、アプリは参照だけを行う。

        /**
         * @brief サーボ操作の結果。通信への応答は呼出元が行う。
         */
        enum class Result {
            Ok,
            Range,
            Unavailable,
            NotArmed,
            IoError,
            WrongMode
        };
        /**
         * @brief I2Cバスと教材のサーボ設定を保持する。
         * @param wire 教材側で開始するI2Cバス。
         * @param config 教材側から渡す設定。
         */
        Pca9685Servos(TwoWire &wire, const Config &config);
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        Pca9685Servos(const Pca9685Servos &) = delete;
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        Pca9685Servos &operator=(const Pca9685Servos &) = delete;
        /**
         * @brief PCA9685を初期化し、全CHをOFFにしてPWM周波数を設定する。
         * @details 全CHをOFFにしてから周波数を設定する。起動・再接続で自動ARMしない。
         * @return 初期化と出力停止が成功したらtrue。自動ARMは行わない。
         */
        bool begin();

        /**
         * @brief 有効化中のCHがあるか調べる。
         * @return 1つでも有効ならtrue。
         */
        bool anyArmed() const;
        /**
         * @brief 補間が完了するまでの残り時間を計算する。
         * @param channel 出力CH番号。
         * @param now 呼出元の現在時刻（ms、周回あり）。
         * @return 残り時間（ms）。補間中でなければ0。
         */
        uint16_t remaining(uint8_t channel, uint32_t now) const;
        /**
         * @brief 指定CHを有効化し、直ちに角度指令を出力する。
         * @param channel 出力CH番号。
         * @param angle 角度（0.01度）。
         * @param duration 移動時間（ms）。0は即時出力。
         * @return 範囲検査と出力の結果。
         */
        Result arm(uint8_t channel, uint16_t angle, uint16_t duration = 0);
        /**
         * @brief 有効なCHの角度を補間する。時間0なら直ちに出力する。
         * @param channel 出力CH番号。
         * @param angle 角度（0.01度）。
         * @param duration 移動時間（ms）。0は即時出力。
         * @param now 呼出元の現在時刻（ms、周回あり）。
         * @return 範囲検査、有効状態と出力の結果。
         */
        Result move(uint8_t channel, uint16_t angle, uint16_t duration, uint32_t now);
        /** @brief 度単位で角度CHを有効化する。@param channel CH0〜15。@param degrees 度。@return 検査・出力結果。 */
        Result armAngle(uint8_t channel, float degrees = 90);
        /** @brief 中立パルス（速度0%）で速度CHを有効化する。@param channel CH0〜15。@return 検査・出力結果。 */
        Result armSpeed(uint8_t channel);
        /** @brief 有効な角度CHを補間する。@param channel CH0〜15。@param degrees 度。@param duration 移動時間ms。@param now 現在時刻ms。@return 検査・出力結果。 */
        Result moveAngle(uint8_t channel, float degrees, uint16_t duration, uint32_t now = millis());
        /** @brief 有効な速度CHへ即時に指令する。@param channel CH0〜15。@param percent −100〜100%。0は中立。@return 検査・出力結果。 */
        Result setSpeed(uint8_t channel, float percent);

        /**
         * @brief 指定CHの補間を止め、PWMを無効にする。
         * @param channel 出力CH番号。
         * @return 停止処理の結果。
         */
        Result disable(uint8_t channel);
        /**
         * @brief 全CHを無効化し、初期化済みPCAへ停止を書き込む。
         */
        void allOff();
        /**
         * @brief 20ms以上の間隔で全CHの補間と出力を進める。
         * @details 補間を進め、失敗したCHを返す。成功または更新なしなら-1。
         * @param now 呼出元の現在時刻（ms、周回あり）。
         * @return 出力に失敗したCH。正常または更新不要なら-1。
         */
        int tick(uint32_t now);

        private:
        /**
         * @brief CH、利用可否、角度と移動時間を検査する。
         * @param channel 出力CH番号。
         * @param angle 角度（0.01度）。
         * @param duration 移動時間（ms）。0は即時出力。
         * @return 最初に見つかったエラー、またはOk。
         */
        Result validate(uint8_t channel, uint16_t angle, uint16_t duration) const;
        /**
         * @brief 角度をPWMパルスへ変換して出力する。失敗時は全CHの停止を試みる。
         * @param channel 出力CH番号。
         * @param angle 角度（0.01度）。
         * @return PWMの書き込みに成功したらtrue。
         */
        bool output(uint8_t channel, uint16_t angle);
        /** @brief パルス幅を出力し、失敗時は全CH停止を試みる。@param channel CH。@param microseconds µs。@return 成功ならtrue。 */
        bool outputPulse(uint8_t channel, float microseconds);
        /** @brief 中立・変化幅・方向を検査する。@param channel CH。@return 有効ならtrue。 */
        bool validSpeedConfig(uint8_t channel) const;
        TwoWire &wire_;
        const Config config_;
        Adafruit_PWMServoDriver pwm_;
        bool initialized_ = false;
        uint32_t lastMotion_ = 0;
    };
}
