/**
 * @file uroa_imu.h
 * @brief MPU-6050の探索、初期化と生データ取得を提供する。
 */
#pragma once
#include <Wire.h>

namespace uroa {
    /**
     * @brief 教材が開始したI2Cバスを使い、単一の制御タスクからIMUを操作する。
     * @details MPU-6050の探索・設定・生データ取得。I2Cは教材側で一度だけ開始する。
     */
    class Mpu6050 {
        public:
        bool available = false; ///< 初期化・直近のI2C処理の結果。アプリは参照だけを行う。
        uint8_t address = 0x68; ///< begin()が選んだI2Cアドレス。変更はbegin()に任せる。

        /**
         * @brief 教材が開始するI2Cバスへの参照を保持する。
         * @param wire 教材側で開始するI2Cバス。
         */
        explicit Mpu6050(TwoWire &wire);

        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        Mpu6050(const Mpu6050 &) = delete;
        /**
         * @brief コピーによる状態の複製を禁止する。
         */
        Mpu6050 &operator=(const Mpu6050 &) = delete;
        /**
         * @brief 0x68/0x69を探索し、100Hz・加速度±2g・角速度±250度/秒へ設定する。
         * @return 識別と全設定の書き込みに成功したらtrue。
         */
        bool begin();

        /**
         * @brief 加速度・温度・角速度の14バイトを取得する。失敗後の復帰にはbegin()が必要。
         * @details 加速度6・温度2・角速度6の14バイトをセンサーのbig endianのまま返す。 読取失敗後はavailable=false。再試行には明示的にbegin()を呼ぶ。
         * @param raw 加速度・温度・角速度の14バイトを受け取る配列。
         * @return 全データの取得に成功したらtrue。
         */
        bool readRaw(uint8_t (&raw)[14]);

        private:
        /**
         * @brief 指定I2Cアドレスから応答があるか調べる。
         * @param address 探索するI2Cアドレス。
         * @return 応答があればtrue。
         */
        bool probe(uint8_t address);
        /**
         * @brief IMUのレジスターへ1バイトを書き込む。
         * @param reg 先頭レジスター番号。
         * @param value 検査または変換する値。
         * @return I2C書き込みが成功したらtrue。
         */
        bool writeRegister(uint8_t reg, uint8_t value);
        /**
         * @brief IMUの指定レジスターから連続して読み取る。
         * @param reg 先頭レジスター番号。
         * @param data 入力または出力のバイト配列。
         * @param count 読み取るバイト数。
         * @return 要求した全バイトを取得できたらtrue。
         */
        bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count);
        TwoWire &wire_;
    };
}
