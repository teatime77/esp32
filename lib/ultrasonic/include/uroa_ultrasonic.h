/**
 * @file uroa_ultrasonic.h
 * @brief HC-SR04のGPIO初期化と距離測定を提供する。
 */
#pragma once
#include <Arduino.h>
#include <math.h>

namespace uroa {
    /**
     * @brief HC-SR04を単一の呼出元タスクから同期的に測定する。
     * @details 通信・周期処理は呼出元が担当する。ECHOは3.3Vへ降圧して接続する。
     * read()は最大約30ms待機するため、割り込みや通信コールバックから呼ばない。
     */
    class HcSr04 {
        public:
        /** @brief 教材側が選ぶGPIO番号。ArduinoのD番号ではない。 */
        struct Config {
            int triggerPin = -1; ///< TRIG用の出力可能なGPIO。
            int echoPin = -1; ///< 降圧したECHOを受ける入力可能なGPIO。
        };

        /** @brief 測定結果。Okの場合だけ新しい距離を利用する。 */
        enum class Result {
            Ok,
            NotInitialized,
            TooSoon,
            EchoHigh,
            Timeout,
            OutOfRange
        };

        bool initialized = false; ///< GPIO設定済みか。begin()/end()が更新し、呼出元は参照だけを行う。
        float distanceCm = NAN; ///< 距離（cm）。begin()/read()/end()が更新し、呼出元は代入しない。
        uint32_t echoDurationUs = 0; ///< ECHO幅（us）。begin()/read()/end()が更新し、呼出元は代入しない。

        /**
         * @brief 教材側が選んだGPIO設定をコピーして保持する。GPIO操作は行わない。
         * @param config TRIGとECHOのGPIO設定。
         */
        explicit HcSr04(const Config &config);

        /** @brief GPIOを共有するコピーを禁止する。 */
        HcSr04(const HcSr04 &) = delete;
        /** @brief GPIOを共有するコピー代入を禁止する。 */
        HcSr04 &operator=(const HcSr04 &) = delete;

        /**
         * @brief GPIOを検査し、TRIGをLOW出力、ECHOを入力に設定する。
         * @details 結果を初期化し、最初の測定まで65msの待機期間を設ける。
         * センサーの接続・応答を検出する処理ではない。
         * @return GPIOが有効で互いに異なればtrue。
         */
        bool begin();

        /**
         * @brief 10usのトリガーを送り、ECHO幅をcmへ変換する。
         * @details begin()または前回の測定開始から65ms未満ならTooSoonで即座に戻る。
         * NotInitialized/TooSoonでは測定値を変更しない。実測失敗では距離をNANにする。
         * ECHO待機のタイムアウトは30000us。割り込み・タスク切替による時間誤差は含まれる。
         * @return 成功、未初期化、間隔不足、ECHOの事前HIGH、タイムアウト、範囲外のいずれか。
         */
        Result read();

        /**
         * @brief TRIGをLOWにして入力へ戻し、初期化状態と測定値を消去する。
         * @details 未初期化でも呼べる。同じタスクから呼ぶ。電源は遮断しない。
         */
        void end();

        private:
        Config config_; ///< コンストラクターで保持し、以降は変更しない。
        uint32_t lastAttemptMs_ = 0; ///< begin()またはread()の測定開始時刻（ms、周回あり）。
    };
}
