/**
 * @file uroa_imu.cpp
 * @brief MPU-6050の探索、初期化と生データ取得を提供する。
 */
#include "uroa_imu.h"

namespace uroa {
    /**
     * @brief 教材が開始するI2Cバスへの参照を保持する。
     * @param wire 教材側で開始するI2Cバス。
     */
    Mpu6050::Mpu6050(TwoWire &wire) : wire_(wire) {
    }

    /**
     * @brief 指定I2Cアドレスから応答があるか調べる。
     * @param address 探索するI2Cアドレス。
     * @return 応答があればtrue。
     */
    bool Mpu6050::probe(uint8_t address) {
        wire_.beginTransmission(address);
        return wire_.endTransmission() == 0;
    }

    /**
     * @brief IMUのレジスターへ1バイトを書き込む。
     * @param reg 先頭レジスター番号。
     * @param value 検査または変換する値。
     * @return I2C書き込みが成功したらtrue。
     */
    bool Mpu6050::writeRegister(uint8_t reg, uint8_t value) {
        wire_.beginTransmission(address);
        wire_.write(reg);
        wire_.write(value);
        return wire_.endTransmission() == 0;
    }

    /**
     * @brief IMUの指定レジスターから連続して読み取る。
     * @param reg 先頭レジスター番号。
     * @param data 入力または出力のバイト配列。
     * @param count 読み取るバイト数。
     * @return 要求した全バイトを取得できたらtrue。
     */
    bool Mpu6050::readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
        wire_.beginTransmission(address);
        wire_.write(reg);
        if (wire_.endTransmission(false) != 0 || wire_.requestFrom(address, count) != count) {
            return false;
        }
        for (uint8_t i = 0; i < count; ++i) {
            data[i] = wire_.read();
        }
        return true;
    }

    /**
     * @brief 0x68/0x69を探索し、100Hz・加速度±2g・角速度±250度/秒へ設定する。
     * @return 識別と全設定の書き込みに成功したらtrue。
     */
    bool Mpu6050::begin() {
        available = false;
        address = 0x68;
        if (!probe(0x68)) {
            if (!probe(0x69)) {
                return false;
            }
            address = 0x69;
        }
        uint8_t who = 0;
        if (!readRegisters(0x75, &who, 1) || who != 0x68) {
            return false;
        }
        // PLL、DLPF約44Hz、内部100Hz、加速度±2g、角速度±250度/秒。
        available = writeRegister(0x6B, 0x01) && writeRegister(0x1A, 0x03) && writeRegister(0x19, 9) &&
            writeRegister(0x1B, 0) && writeRegister(0x1C, 0);
        return available;
    }

    /**
     * @brief 加速度・温度・角速度の14バイトを取得する。失敗後の復帰にはbegin()が必要。
     * @param raw 加速度・温度・角速度の14バイトを受け取る配列。
     * @return 全データの取得に成功したらtrue。
     */
    bool Mpu6050::readRaw(uint8_t (&raw)[14]) {
        if (!available) {
            return false;
        }
        available = readRegisters(0x3B, raw, sizeof raw);
        return available;
    }
}
