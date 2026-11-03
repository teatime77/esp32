/** @file bytecode_vm.h
 * @brief 受信・検証済みのPythonバイトコードversion=1を協調実行するVM。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>

/** @brief loop()だけが開始・停止・実行する数値スタックVM。機器APIは呼ばない。 */
class BytecodeVM {
    public:
    /**
     * @details
     * ## 上限と公開状態
     *
     * 大域変数は最大1024個、スタックは管理領域・引数・局所変数を含め1024スロット、同時関数呼出しは64段。超過や動的確保の失敗は実行エラーとする。これらはバイナリ形式のuint16上限とは別の、現在のVMの実行上限。
     */
    static constexpr uint32_t MaxStack = 1024; ///< 引数・管理領域・局所変数・一時値の合計上限。
    static constexpr uint32_t MaxGlobals = 1024; ///< 大域変数の上限。
    static constexpr uint32_t MaxFrames = 64; ///< 同時に呼出せる関数の深さ。

    /** @brief VMの実行状態。更新はVMに任せる。
     *
     * @details
     * stateは0=Idle、1=Running、2=Halted、3=Stopped、4=Error。
     */
    enum class State : uint8_t {
        Idle,
        Running,
        Halted,
        Stopped,
        Error
    };
    /** @brief 実行時エラー。errorPcは失敗した命令の先頭を指す。
     *
     * @details
     * | error | 意味 |
     * |---|---|
     * | 0 | エラーなし |
     * | 1 | ヘッダー・命令形式不正 |
     * | 2 | メモリ確保失敗 |
     * | 3 | 大域変数数の上限超過 |
     * | 4 | 一時スタック不足 |
     * | 5 | スタック上限超過 |
     * | 6 | 関数呼出し深さの上限超過 |
     * | 7 | 変数位置・PC等の不正 |
     * | 8 | 未初期化変数の読出し |
     * | 9 | 型エラー |
     * | 10 | 0除算 |
     * | 11 | 整数オーバーフロー |
     * | 12 | 非有限float |
     * | 13 | RETまたは保存済み戻り先の不正 |
     */
    enum class Error : uint8_t {
        None,
        Format,
        Memory,
        Limit,
        StackUnderflow,
        StackOverflow,
        FrameOverflow,
        Operand,
        Uninitialized,
        Type,
        DivisionByZero,
        IntegerOverflow,
        NonFinite,
        Return
    };
    /** @brief 値の種別。末尾2種は関数フレームの管理用で、Pythonから読み書きできない。
     *
     * @details
     * ## 値と演算
     *
     * 値はint32、float32、None、未初期化を区別する。
     *
     * True／False・比較結果は整数1／0。
     */
    enum class Type : uint8_t {
        Uninitialized,
        Integer,
        Float,
        None,
        ReturnPc,
        SavedBp
    };
    /** @brief VMの1スロット。数値・None・未初期化・管理情報を区別する。 */
    struct Value {
        Type type = Type::Uninitialized;
        union {
            int32_t integer = 0;
            float real;
            uint32_t address;
        };
    };

    /**
     * @details
     * `state`、`error`、`errorPc`、`pc`、`sp`、`bp`、`instructions`、`globals`、`globalCount`は外部から参照だけを行う。大域変数は正常終了・エラー・停止後も保持し、次のstart()で初期化する。
     */
    State state = State::Idle; ///< 外部は参照のみ。loop()からのVM操作で更新する。
    Error error = Error::None; ///< 最後のエラー。start()で消去する。
    uint32_t errorPc = 0; ///< エラー命令のPC。
    uint32_t pc = 0; ///< 命令列先頭からのバイト位置。
    uint32_t sp = 0; ///< 次にpushするスタックスロット。
    uint32_t bp = 0; ///< 現関数の管理領域の先頭。
    uint32_t instructions = 0; ///< 今回の実行で処理した命令数。
    std::unique_ptr<Value[]> globals; ///< 実行結果。HALT・エラー・停止後も保持する。外部は参照のみ。
    uint32_t globalCount = 0; ///< globalsの有効要素数。

    /**
     * @brief 受信器で構造検証済みのイメージを借用して実行を開始する。
     * @param image バイナリ全体。tick()中は不変とし、置換後はtick()前にstart()かstop()を呼ぶ。
     * @param length 全体長。グローバル変数と実行スタックは新規初期化する。
     * @return 実行を開始できたらtrue。
     *
     * @details
     * start()の入力は受信器で命令境界・分岐先等を全件検証した不変バッファに限る。VMはさらに実行時の命令範囲・変数位置・スタック・型を検査する。検証前の任意のデータをstart()へ直接渡さない。
     */
    bool start(const uint8_t *image, size_t length);
    /** @brief 最大budget命令を進める。@param budget 1回のloop()に割り当てる命令数。 */
    void tick(uint32_t budget = 128);
    /** @brief 実行を停止してイメージの参照を解除する。大域変数と診断情報は保持する。 */
    void stop();

    private:
    /** @brief 呼出元の命令範囲と一時スタックの下限。戻り先PC・旧BPは値スタックに置く。 */
    struct Frame {
        uint32_t base;
        uint32_t end;
        uint32_t floor;
    };
    /** @brief エラー状態へ移る。@param code 失敗理由。@return 常にfalse。 */
    bool fail(Error code);
    /** @brief 1命令を実行する。 */
    void step();
    /** @brief 値を積む。@param value Pythonの値。@return 成功時true。 */
    bool push(const Value &value);
    /** @brief 値を取り出す。@param value 出力先。@return 成功時true。 */
    bool pop(Value &value);
    /** @brief 数値型を調べる。@param value 対象。@return intまたはfloatならtrue。 */
    static bool numeric(const Value &value);
    /** @brief 条件値を変換する。@param value 対象。@param result 真偽。@return 有効な値ならtrue。 */
    bool truth(const Value &value, bool &result);
    /** @brief 四則・比較を計算する。@param opcode 命令番号。 */
    void calculate(uint8_t opcode);
    /** @brief 関数フレームを作る。@param entry 関数入口PC。 */
    void call(uint16_t entry);
    /** @brief フレームを解放し、呼出元へ戻り値を1個積む。 */
    void ret();

    const uint8_t *table_ = nullptr;
    const uint8_t *code_ = nullptr;
    uint32_t codeLength_ = 0;
    uint16_t functionCount_ = 0;
    std::unique_ptr<Value[]> stack_;
    Frame frames_[MaxFrames]{};
    uint32_t depth_ = 0;
    uint32_t base_ = 0;
    uint32_t end_ = 0;
    uint32_t floor_ = 0;
};
