/** @file bytecode_vm.h
 * @brief Pythonの値スロットVM。呼出し・所有権の規約はbytecode_vm.cppの各処理を参照。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>

/** @brief loop()だけが操作するVM。機器の実装は外部のNativeApiから接続する。 */
class BytecodeVM {
    public:
    static constexpr uint32_t MaxStack = 1024; ///< 管理領域・引数・局所・一時値の上限。
    static constexpr uint32_t MaxGlobals = 1024; ///< 展開後の大域スロット上限。
    static constexpr uint32_t MaxFrames = 64; ///< 同時ユーザー関数呼出し上限。
    static constexpr uint32_t MaxStrings = 64; ///< 動的文字列の同時保持上限。
    static constexpr uint32_t MaxStringLength = 1024; ///< 整形・連結結果の最大UTF-8バイト長。
    /** @brief 実行状態。従来の番号0〜4を維持する。 */
    enum class State : uint8_t {
        Idle,
        Running,
        Halted,
        Stopped,
        Error,
        Paused ///< 5: ブレークポイントの命令を未実行のまま値と機器ハンドルを保持する。
    };
    /** @brief 実行エラー。0〜13は従来通り。errorPcが失敗命令を示す。 */
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
        Return,
        NativeUnavailable, ///< 14: スタブ未登録、またはIDがない。
        AbiMismatch, ///< 15: バイトコードと登録表のAPIハッシュが違う。
        NativeFailure, ///< 16: スタブが報告する機器エラー等。
        StringLimit, ///< 17: 動的文字列数・長さの上限。
        StringFormat, ///< 18: 未対応・不正な数値書式。
        NativePending ///< 19: スタブからVMへの待機通知。Error状態にはせず、次のtickで再試行する。
    };
    /** @brief スロット型。管理用のReturnPc/SavedBpはPythonに公開しない。 */
    enum class Type : uint8_t {
        Uninitialized,
        Integer,
        Float,
        None,
        ReturnPc,
        SavedBp,
        String,
        Object
    };
    /** @brief 1個の値。構造体は宣言順に複数スロットを並べる。 */
    struct Value {
        Type type = Type::Uninitialized;
        uint16_t classId = 0; ///< ObjectのクラスID。ほかの型では使用しない。
        union {
            int32_t integer = 0;
            float real;
            uint32_t address; ///< 管理値・文字列ID・機器ハンドル。C++ポインターではない。
        };
    };
    /** @brief スタブ呼出し領域。arguments/resultsは同期コールバック中だけ有効。 */
    struct NativeCall {
        BytecodeVM &vm; ///< stringBytes()による文字列参照に使用。再入・VM操作は禁止。
        const Value *arguments; ///< self、明示引数の順。構造体は展開済み。
        Value *results; ///< 必ずresultSlots個すべてを設定する。Noneも1個返す。
        uint16_t argumentSlots;
        uint16_t resultSlots;
        void *context; ///< 登録元が所有する機器管理状態。
    };
    /** @brief 単一nativeIdとスタブ・入出力スロット幅の対応。 */
    struct NativeEntry {
        uint16_t id;
        uint16_t argumentSlots;
        uint16_t resultSlots;
        Error (*invoke)(NativeCall &call); ///< 同期実行。成功時Error::None。
    };
    /** @brief 自動生成する登録表。表・ハッシュ・contextは登録中ずっと有効に保つ。 */
    struct NativeApi {
        const uint8_t *abiHash; ///< JSONのabiHashを復号した32バイト。
        const NativeEntry *entries; ///< 非0のID昇順、重複なし。
        size_t count;
        void *context;
        void (*reset)(void *context); ///< 任意。stop/再開始/登録変更時に機器ハンドルを解放する。
    };

    State state = State::Idle; ///< 外部は参照のみ。以下の診断情報はVMが更新する。
    Error error = Error::None;
    uint32_t errorPc = 0;
    uint32_t pc = 0;
    uint32_t sp = 0;
    uint32_t bp = 0;
    uint32_t instructions = 0;
    std::unique_ptr<Value[]> globals; ///< stopではString/Objectを無効化し、数値は保持する。
    uint32_t globalCount = 0;

    /** @brief 停止中にスタブ表を検証・登録する。@param api nullで解除。@return 登録できればtrue。 */
    bool bindNativeApi(const NativeApi *api);
    /** @brief 構造検証済みイメージを借用して開始する。
     * @param image BytecodeDebuggerによる命令先頭bit7以外は不変のバッファ。
     * @param length 全長。@return 成功時true。
     */
    bool start(const uint8_t *image, size_t length);
    /** @brief 最大budget命令を実行する。@param budget loop()1回の割当量。 */
    void tick(uint32_t budget = 128);
    /** @brief 借用参照・動的文字列・機器ハンドルを解放する。数値と診断情報は保持する。 */
    void stop();
    /** @brief Stringを検査して長さ付きUTF-8を借用する。@param value 対象。@param data 出力先。@param length バイト長。@return 有効ならtrue。 */
    bool stringBytes(const Value &value, const char *&data, size_t &length) const;
    /** @brief 停止値と参照中の文字列を含むSnapshot長を計算する。@return 不正状態・上限超過時0。 */
    size_t debugSnapshotLength() const;
    /** @brief 一時停止中の値を通信形式へコピーする。@param data 出力。@param length debugSnapshotLength()の値。@return 成功時true。 */
    bool copyDebugSnapshot(uint8_t *data, size_t length) const;

    private:
    /** @brief 使用スロット順で文字列IDの初出か調べる。@param index 大域、スタックの順の添字。@return 初出Stringならtrue。 */
    bool firstDebugString(uint32_t index) const;
    /** @brief 呼出し元の命令範囲・一時値下限・戻り値幅。 */
    struct Frame {
        uint32_t base;
        uint32_t end;
        uint32_t floor;
        uint32_t results;
    };
    /** @brief プール内の文字列。Valueは添字だけを保持する。 */
    struct Text {
        std::unique_ptr<char[]> bytes;
        size_t length = 0;
    };
    /** @brief エラー状態へ移る。@param code 理由。@return false。 */
    bool fail(Error code);
    /** @brief 1命令を実行する。 */
    void step();
    /** @brief 値を積む。@param value 値。@return 成功時true。 */
    bool push(const Value &value);
    /** @brief 固定領域を侵さず値を取り出す。@param value 出力先。@return 成功時true。 */
    bool pop(Value &value);
    /** @brief 数値型か検査する。@param value 値。@return 数値ならtrue。 */
    static bool numeric(const Value &value);
    /** @brief 外部から返せるスロットか検査する。@param value 値。@return 有効ならtrue。 */
    bool validValue(const Value &value) const;
    /** @brief 条件値を変換する。@param value 値。@param result 真偽。@return 成功時true。 */
    bool truth(const Value &value, bool &result);
    /** @brief 四則・比較を行う。@param opcode 命令番号。 */
    void calculate(uint8_t opcode);
    /** @brief ユーザー関数フレームを作る。@param entry 入口PC。 */
    void call(uint16_t entry);
    /** @brief 宣言された幅の戻り値を呼出し元へコピーする。 */
    void ret();
    /** @brief 登録表のスタブを同期実行する。@param id 単一ネイティブID。 */
    void callNative(uint16_t id);
    /** @brief 到達不能文字列を回収して結果領域を確保する。@param length バイト長。@param value 出力値。@param bytes 書込先。@return 成功時true。 */
    bool allocateText(size_t length, Value &value, char *&bytes);
    /** @brief 数値を整形する。@param format 定数文字列ID。 */
    void formatValue(uint16_t format);
    /** @brief スタック上の断片を連結する。@param count 断片数。 */
    void buildString(uint16_t count);

    const NativeApi *nativeApi_ = nullptr;
    bool nativeActive_ = false;
    bool yielded_ = false; ///< ネイティブ呼出し後はloop()へ戻す。tick()が更新する。
    uint8_t abiHash_[32]{};
    uint16_t version_ = 0;
    size_t recordLength_ = 8;
    const uint8_t *table_ = nullptr;
    const uint8_t *code_ = nullptr;
    const uint8_t *strings_ = nullptr;
    const uint8_t *stringData_ = nullptr;
    uint16_t stringCount_ = 0;
    uint32_t stringDataLength_ = 0;
    uint32_t codeLength_ = 0;
    uint16_t functionCount_ = 0;
    std::unique_ptr<Value[]> stack_;
    Text texts_[MaxStrings];
    Frame frames_[MaxFrames]{};
    uint32_t depth_ = 0;
    uint32_t base_ = 0;
    uint32_t end_ = 0;
    uint32_t floor_ = 0;
    uint32_t resultSlots_ = 1;
};
