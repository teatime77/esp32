/**
 * @file uroa_display.h
 * @brief SSD1306の描画と分割転送を提供する。実装はuroa_display.cpp。
 * @details VMのハンドル・文字列・エラー変換はapp/diagram-control/src/device_native.cppが担当する。
 */
#pragma once
#include <Adafruit_SSD1306.h>

namespace uroa {
    /**
     * @brief 開始済みI2Cバスを単一タスクから使うSSD1306ドライバー。
     * @details 描画はRAMだけを変更し、displayStep()で画面へ送る。Wire.begin()は呼ばない。
     * 転送中は描画せず、Pendingの間は同じタスクからdisplayStep()を繰り返す。
     * 対応寸法は128x64、128x32、96x16、64x32。外部リセットピンとSPIは扱わない。
     * 座標は−1024〜1024、色は0〜2。画面外の描画はGFXが切り詰める。
     */
    class Ssd1306Display {
        public:
        /** @brief 教材が決める画面と共有バスの設定。構築後は変更しない。 */
        struct Config {
            uint8_t width = 128; ///< 横画素数。
            uint8_t height = 64; ///< 縦画素数。
            uint8_t address = 0x3C; ///< 0x3Cまたは0x3D。
            uint32_t i2cClockHz = 100000; ///< 初期化中・初期化後とも使う共有バスのクロック。
        };
        /** @brief 描画・初期化・分割転送の結果。 */
        enum class Result : uint8_t {
            Ok,
            Pending,
            InvalidArgument,
            NotInitialized,
            NoMemory,
            I2cError
        };
        bool available = false; ///< begin/reset/displayStepが更新する。呼出元は参照だけを行う。

        /** @brief バスを借用し、設定をコピーする。I2C操作・バッファ確保は行わない。@param wire 開始済みバス。@param config 教材の設定。 */
        Ssd1306Display(TwoWire &wire, const Config &config);
        /** @brief 表示バッファの共有所有を防ぐ。 */
        Ssd1306Display(const Ssd1306Display &) = delete;
        /** @brief 表示バッファの共有所有を防ぐ。 */
        Ssd1306Display &operator=(const Ssd1306Display &) = delete;
        /**
         * @brief 応答を検査し、内部昇圧電源で初期化する。@return 初期化結果。
         * @details 白文字・黒背景・標準GFXフォント・倍率1・折返しあり・カーソル(0,0)・空バッファへ戻す。
         * 画面への送信はdisplayStep()で行う。Adafruitの初期化コマンド個別のACKは取得できない。
         */
        Result begin();
        /** @brief 転送を中止し未初期化へ戻す。確保済みRAMと画面の表示は保持する。 */
        void reset();
        /** @brief RAMの全画素を消し、カーソルを(0,0)へ戻す。@return 操作結果。 */
        Result clear();
        /** @brief 文字色を設定する。白文字は黒背景、黒・反転は背景透明。@param color 0=黒、1=白、2=反転。@return 操作結果。 */
        Result setTextColor(int32_t color);
        /** @brief 文字の開始位置を設定する。@param x 横座標。@param y 縦座標。@return 操作結果。 */
        Result setCursor(int32_t x, int32_t y);
        /**
         * @brief 長さ付きバイト列をRAMへ描く。改行は追加しない。@param text 文字列。@param length バイト数。@return 操作結果。
         * @details NUL終端に依存しない。標準GFXフォントを使い、日本語フォント・UTF-8変換は行わない。
         * 白文字は空白を含む各6×8画素の文字セルを黒背景で上書きし、以前の字形との重なりを防ぐ。
         * 改行・setCursor()は消去しない。短い文字列へ更新して余る部分は、末尾の空白かclear()で消す。
         * 黒・反転は従来どおり字形の画素だけへ作用する。
         * 全バイトを同期処理するため、VM側は1呼出し64バイト以下へ分割する。
         */
        Result print(const char *text, size_t length);
        /** @brief 線を描く。@param x1 始点X。@param y1 始点Y。@param x2 終点X。@param y2 終点Y。@param color 色。@return 操作結果。 */
        Result drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color);
        /** @brief 矩形を描く。@param x 左。@param y 上。@param width 幅0〜1024。@param height 高さ0〜1024。@param color 色。@return 操作結果。 */
        Result drawRect(int32_t x, int32_t y, int32_t width, int32_t height, int32_t color);
        /** @brief 円を描く。@param x 中心X。@param y 中心Y。@param radius 半径0〜1024。@param color 色。@return 操作結果。 */
        Result drawCircle(int32_t x, int32_t y, int32_t radius, int32_t color);
        /** @brief 円を塗る。@param x 中心X。@param y 中心Y。@param radius 半径0〜1024。@param color 色。@return 操作結果。 */
        Result fillCircle(int32_t x, int32_t y, int32_t radius, int32_t color);
        /**
         * @brief 最大16バイトの画素を転送する。@return 続きがあればPending、完了ならOk、失敗ならI2cError。
         * @details 初回は水平アドレス範囲も送る。I2C失敗時は転送中止・available=falseとし、復帰にはbegin()が必要。
         */
        Result displayStep();

        private:
        /** @brief 描画可能状態と色を検査する。@param color 色。@return 操作結果。 */
        Result drawable(int32_t color) const;
        /** @brief 座標の演算量とint16範囲を制限する。@param value 座標。@return −1024〜1024ならtrue。 */
        static bool coordinate(int32_t value);
        /** @brief I2Cへ制御バイトとデータを送る。@param control SSD1306制御バイト。@param data バイト列。@param length 長さ。@return 全書込とACKが成功したらtrue。 */
        bool send(uint8_t control, const uint8_t *data, size_t length);
        TwoWire &wire_;
        const Config config_;
        Adafruit_SSD1306 display_;
        bool flushing_ = false;
        size_t displayOffset_ = 0;
    };
}
