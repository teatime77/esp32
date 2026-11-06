/** @file uroa_display.cpp
 * @brief uroa_display.hのSSD1306描画・初期化・分割転送を実装する。
 */
#include "uroa_display.h"
#include <algorithm>

namespace uroa {
    /** @brief バスと設定を保持する。@param wire 開始済みバス。@param config 教材の設定。 */
    Ssd1306Display::Ssd1306Display(TwoWire &wire, const Config &config)
        : wire_(wire), config_(config),
          display_(config.width, config.height, &wire, -1, config.i2cClockHz, config.i2cClockHz) {
    }

    /** @brief 未初期化へ戻し、途中の転送を破棄する。画面へのI2C操作は行わない。 */
    void Ssd1306Display::reset() {
        available = false;
        flushing_ = false;
        displayOffset_ = 0;
    }

    /** @brief 設定・応答・メモリーを検査して初期化する。@return 初期化結果。 */
    Ssd1306Display::Result Ssd1306Display::begin() {
        reset();
        const bool supportedSize = (config_.width == 128 && (config_.height == 64 || config_.height == 32)) ||
            (config_.width == 96 && config_.height == 16) || (config_.width == 64 && config_.height == 32);
        if (!supportedSize || (config_.address != 0x3C && config_.address != 0x3D) || config_.i2cClockHz == 0) {
            return Result::InvalidArgument;
        }
        wire_.beginTransmission(config_.address);
        if (wire_.endTransmission() != 0) {
            return Result::I2cError;
        }
        if (!display_.begin(SSD1306_SWITCHCAPVCC, config_.address, false, false)) {
            return Result::NoMemory;
        }
        display_.clearDisplay();
        display_.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display_.setTextSize(1);
        display_.setCursor(0, 0);
        display_.setTextWrap(true);
        available = true;
        return Result::Ok;
    }

    /** @brief 初期化と色を検査する。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::drawable(int32_t color) const {
        if (!available) {
            return Result::NotInitialized;
        }
        if (color < 0 || color > 2) {
            return Result::InvalidArgument;
        }
        return Result::Ok;
    }

    /** @brief 極端な座標による長時間の描画を避ける。@param value 座標。@return 有効ならtrue。 */
    bool Ssd1306Display::coordinate(int32_t value) {
        return value >= -1024 && value <= 1024;
    }

    /** @brief 空バッファと原点へ戻す。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::clear() {
        if (!available) {
            return Result::NotInitialized;
        }
        display_.clearDisplay();
        display_.setCursor(0, 0);
        return Result::Ok;
    }

    /** @brief 後続の文字色を設定する。白文字は背景を黒で上書きし、黒・反転は字形だけを描く。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::setTextColor(int32_t color) {
        const Result status = drawable(color);
        if (status != Result::Ok) {
            return status;
        }
        const uint16_t background = color == SSD1306_WHITE ? SSD1306_BLACK : uint16_t(color);
        display_.setTextColor(uint16_t(color), background);
        return Result::Ok;
    }

    /** @brief 文字の開始位置を設定する。@param x 横。@param y 縦。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::setCursor(int32_t x, int32_t y) {
        if (!available) {
            return Result::NotInitialized;
        }
        if (!coordinate(x) || !coordinate(y)) {
            return Result::InvalidArgument;
        }
        display_.setCursor(int16_t(x), int16_t(y));
        return Result::Ok;
    }

    /** @brief 長さ付き文字列を描く。@param text バイト列。@param length 長さ。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::print(const char *text, size_t length) {
        if (!available) {
            return Result::NotInitialized;
        }
        if (!text && length != 0) {
            return Result::InvalidArgument;
        }
        if (length != 0) {
            display_.write(reinterpret_cast<const uint8_t *>(text), length);
        }
        return Result::Ok;
    }

    /** @brief 線を描く。@param x1 始点X。@param y1 始点Y。@param x2 終点X。@param y2 終点Y。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color) {
        const Result status = drawable(color);
        if (status != Result::Ok) {
            return status;
        }
        if (!coordinate(x1) || !coordinate(y1) || !coordinate(x2) || !coordinate(y2)) {
            return Result::InvalidArgument;
        }
        display_.drawLine(int16_t(x1), int16_t(y1), int16_t(x2), int16_t(y2), uint16_t(color));
        return Result::Ok;
    }

    /** @brief 矩形を描く。@param x 左。@param y 上。@param width 幅。@param height 高さ。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::drawRect(
        int32_t x, int32_t y, int32_t width, int32_t height, int32_t color) {
        const Result status = drawable(color);
        if (status != Result::Ok) {
            return status;
        }
        if (!coordinate(x) || !coordinate(y) || width < 0 || width > 1024 || height < 0 || height > 1024) {
            return Result::InvalidArgument;
        }
        if (width != 0 && height != 0) {
            display_.drawRect(int16_t(x), int16_t(y), int16_t(width), int16_t(height), uint16_t(color));
        }
        return Result::Ok;
    }

    /** @brief 円を描く。@param x 中心X。@param y 中心Y。@param radius 半径。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::drawCircle(int32_t x, int32_t y, int32_t radius, int32_t color) {
        const Result status = drawable(color);
        if (status != Result::Ok) {
            return status;
        }
        if (!coordinate(x) || !coordinate(y) || radius < 0 || radius > 1024) {
            return Result::InvalidArgument;
        }
        display_.drawCircle(int16_t(x), int16_t(y), int16_t(radius), uint16_t(color));
        return Result::Ok;
    }

    /** @brief 円を塗る。@param x 中心X。@param y 中心Y。@param radius 半径。@param color 色。@return 操作結果。 */
    Ssd1306Display::Result Ssd1306Display::fillCircle(int32_t x, int32_t y, int32_t radius, int32_t color) {
        const Result status = drawable(color);
        if (status != Result::Ok) {
            return status;
        }
        if (!coordinate(x) || !coordinate(y) || radius < 0 || radius > 1024) {
            return Result::InvalidArgument;
        }
        display_.fillCircle(int16_t(x), int16_t(y), int16_t(radius), uint16_t(color));
        return Result::Ok;
    }

    /** @brief 制御バイト付きでI2C送信する。@param control 制御バイト。@param data データ。@param length 長さ。@return 書込・ACK成功ならtrue。 */
    bool Ssd1306Display::send(uint8_t control, const uint8_t *data, size_t length) {
        wire_.beginTransmission(config_.address);
        const size_t controlWritten = wire_.write(control);
        const size_t dataWritten = wire_.write(data, length);
        const uint8_t status = wire_.endTransmission();
        return controlWritten == 1 && dataWritten == length && status == 0;
    }

    /** @brief 1回最大16バイトの画素を転送する。@return 続行・完了・エラー。 */
    Ssd1306Display::Result Ssd1306Display::displayStep() {
        if (!available) {
            return Result::NotInitialized;
        }
        if (!flushing_) {
            const uint8_t commands[] = {SSD1306_COLUMNADDR, 0, uint8_t(config_.width - 1), SSD1306_PAGEADDR, 0,
                uint8_t(config_.height / 8 - 1)};
            if (!send(0, commands, sizeof commands)) {
                reset();
                return Result::I2cError;
            }
            displayOffset_ = 0;
            flushing_ = true;
        }
        const size_t length = size_t(config_.width) * config_.height / 8;
        const size_t count = std::min(size_t(16), length - displayOffset_);
        if (!send(0x40, display_.getBuffer() + displayOffset_, count)) {
            reset();
            return Result::I2cError;
        }
        displayOffset_ += count;
        if (displayOffset_ != length) {
            return Result::Pending;
        }
        flushing_ = false;
        return Result::Ok;
    }
}
