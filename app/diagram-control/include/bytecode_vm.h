/**
 * @file bytecode_vm.h
 * @brief Pythonの値スロットVM。呼出し・所有権の規約はbytecode_vm.cppの各処理を参照。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>
#include "bytecode_heap.h"

/**
 * @brief loop()だけが操作するVM。機器の実装は外部のNativeApiから接続する。
 */
class BytecodeVM {
    public:
    static constexpr uint32_t MaxStack = 1024; ///< 管理領域・引数・局所・一時値の上限。
    static constexpr uint32_t MaxGlobals = 1024; ///< 展開後の大域スロット上限。
    static constexpr uint32_t MaxFrames = 64; ///< 同時ユーザー関数呼出し上限。
    static constexpr uint32_t MaxStrings = 64; ///< 動的文字列の同時保持上限。
    static constexpr uint32_t MaxStringLength = 1024; ///< 整形・連結結果の最大UTF-8バイト長。
    static constexpr size_t MaxHeapBytes =
        128 * 1024; ///< GC本体と管理ヘッダーの合計上限。物理的な確保成功は保証しない。
    static constexpr size_t HeapCollectionBytes = 32 * 1024; ///< 回収後の累積確保量が達したら次の命令境界でGCを行う。
    /**
     * @brief 実行状態。従来の番号0〜4を維持する。
     */
    enum class State : uint8_t {
        Idle,
        Running,
        Halted,
        Stopped,
        Error,
        Paused ///< 5: ブレークポイント／ステップの次の命令を未実行のまま値と機器ハンドルを保持する。
    };
    /**
     * @brief 一時停止からの実行方法。通信の10〜13に対応する。
     */
    enum class DebugAction : uint8_t {
        Continue,
        Over,
        In,
        Out
    };
    /**
     * @brief 実行エラー。0〜13は従来通り。errorPcが失敗命令を示す。
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
        Return,
        NativeUnavailable, ///< 14: スタブ未登録、またはIDがない。
        AbiMismatch, ///< 15: バイトコードと登録表のAPIハッシュが違う。
        NativeFailure, ///< 16: スタブが報告する機器エラー等。
        StringLimit, ///< 17: 動的文字列数・長さの上限。
        StringFormat, ///< 18: 未対応・不正な数値書式。
        NativePending ///< 19: スタブからVMへの待機通知。Error状態にはせず、次のtickで再試行する。
    };
    /**
     * @brief スロット型。管理用のReturnPc/SavedBpはPythonに公開しない。
     */
    enum class Type : uint8_t {
        Uninitialized,
        Integer,
        Float,
        None,
        ReturnPc,
        SavedBp,
        String,
        Object,
        HeapReference ///< 8: VM内部のGC本体参照。バイトコード型・Snapshotへの公開は後続段階で行う。
    };

    /**
     * @brief 1個の値。既存recordは複数スロット、GC本体はポインター1個を保持する。
     */
    struct Value {
        Type type = Type::Uninitialized; ///< 未初期化・数値・管理値・文字列・機器・GC本体を区別する。
        uint16_t classId = 0; ///< ObjectまたはHeapReferenceの型ID。ほかの型では使用しない。
        union {
            int32_t integer = 0; ///< Integerの符号付き32bit整数値。初期値は0。
            float real; ///< Floatの単精度値。演算と外部呼出しでは有限性も検査する。
            uint32_t address; ///< 管理値・文字列ID・機器ハンドル。C++ポインターではない。
            BytecodeHeap::Block *reference; ///< HeapReferenceだけが使うGCヘッダーへのポインター。整数から復元しない。
        };
    };

    /**
     * @brief C++側のValueをGCの根へ登録するスコープガード。参照カウントではない。
     * @details 値はガードより長く生存させ、確保APIを呼ぶ前に登録する。VMの生存中にloop()だけで使用する。
     * Valueの代入後も同じ登録で追跡する。登録前の生ポインター保持や、ガードのコピー・移動は禁止する。
     * C++が所有するValueだけを登録し、VMスロットやGC本体内のValueは登録しない。文字列プールの根登録は対象外。
     */
    class HeapRoot {
        public:
        /**
         * @brief 未初期化の値も含め、C++の一時値を根へ登録する。
         * @param vm 根を管理するVM。
         * @param value 根として借用する値。Stop時は未初期化へ戻される。
         */
        HeapRoot(BytecodeVM &vm, Value &value);
        /**
         * @brief 根から外す。本体はここでは解放しない。
         */
        ~HeapRoot();
        /**
         * @brief 同じ根の二重登録をコピーで作らない。
         * @param other コピー元。
         */
        HeapRoot(const HeapRoot &other) = delete;
        /**
         * @brief 登録済みリンクをコピー代入しない。
         * @param other コピー元。
         * @return 使用禁止。
         */
        HeapRoot &operator=(const HeapRoot &other) = delete;

        private:
        friend class BytecodeVM;
        BytecodeVM &vm_; ///< ガードより長く生存するVM。
        Value &value_; ///< C++側が更新する根の値。
        HeapRoot *previous_ = nullptr; ///< 根リストの直前。
        HeapRoot *next_ = nullptr; ///< 根リストの直後。
    };

    /**
     * @brief スタブ呼出し領域。arguments/resultsは同期コールバック中だけ有効。
     */
    struct NativeCall {
        BytecodeVM &vm; ///< 文字列参照・GC本体確保・HeapRoot登録に使用。実行・停止等のVM再入は禁止。
        const Value *arguments; ///< self、明示引数の順。構造体は展開済み。
        Value *results; ///< 必ずresultSlots個すべてを設定する。Noneも1個返す。
        uint16_t argumentSlots; ///< selfと明示引数を宣言順に展開した入力スロット数。
        uint16_t resultSlots; ///< 戻り値を宣言順に展開した出力スロット数。Noneも1個を使う。
        void *context; ///< 登録元が所有する機器管理状態。
    };
    /**
     * @brief 単一nativeIdとスタブ・入出力スロット幅の対応。
     */
    struct NativeEntry {
        uint16_t id; ///< API定義の全呼出しで共有する非0のnativeId。登録表では昇順に並べる。
        uint16_t argumentSlots; ///< selfと明示引数を宣言順に展開した入力スロット数。
        uint16_t resultSlots; ///< 戻り値を宣言順に展開した出力スロット数。Noneも1個を使う。
        Error (*invoke)(NativeCall &call); ///< 同期実行。成功時Error::None。
    };
    /**
     * @brief 自動生成する登録表。表・ハッシュ・contextは登録中ずっと有効に保つ。
     */
    struct NativeApi {
        const uint8_t *abiHash; ///< JSONのabiHashを復号した32バイト。
        const NativeEntry *entries; ///< 非0のID昇順、重複なし。
        size_t count; ///< entries内の登録件数。bindNativeApi()が上限と並び順を検証する。
        void *context; ///< 登録元が所有し、スタブとresetへ渡す機器管理状態。VMは解放しない。
        void (*reset)(void *context); ///< 任意。stop/再開始/登録変更時に機器ハンドルを解放する。
    };

    State state = State::Idle; ///< 外部は参照のみ。以下の診断情報はVMが更新する。
    Error error = Error::None; ///< VMが記録する実行エラー。外部は参照のみで、start()がNoneへ戻す。
    uint32_t errorPc = 0; ///< step()が記録する実行対象命令のPC。エラー・ネイティブ再試行に使う。
    uint32_t pc = 0; ///< 命令列先頭を0とする次の命令位置。Pausedでは未実行の停止命令を指す。
    uint32_t sp = 0; ///< 値スタックの使用末尾の直後。固定領域と一時値を含むスロット数。
    uint32_t bp = 0; ///< 現在の関数フレームの先頭スロット。BP+0/1は戻りPCと旧BP。
    uint32_t instructions = 0; ///< step()が加算する実行命令数。NativePendingによる再試行も数える。
    uint8_t pauseReason = 1; ///< VMが更新する。1=ブレークポイント、2=ステップ。
    BytecodeHeap::Statistics heapStatistics; ///< ヒープが更新する確保量とGC診断。外部は参照のみ。start()で初期化する。
    std::unique_ptr<Value[]> globals; ///< stopではString/Object/HeapReferenceを無効化し、数値は保持する。
    uint32_t globalCount = 0; ///< start()が確保した大域値スロット数。構造体は末端フィールドの幅を含む。

    /**
     * @brief 停止中にスタブ表を検証・登録する。
     * @param api nullで解除。
     * @return 登録できればtrue。
     */
    bool bindNativeApi(const NativeApi *api);
    /**
     * @brief 構造検証済みイメージを借用して開始する。
     * @param image BytecodeDebuggerによる命令先頭bit7以外は不変のバッファ。
     * @param length 全長。
     * @return 成功時true。
     */
    bool start(const uint8_t *image, size_t length);
    /**
     * @brief 最大budget命令を実行する。
     * @param budget loop()1回の割当量。
     */
    void tick(uint32_t budget = 128);
    /**
     * @brief Value列と数値バイト列からなるGC本体を確保する。Pythonからの生成はまだ未対応。
     * @param classId 非0の本体型ID。
     * @param valueCount 本体先頭に置くValueの個数。参照型のスロットだけをGCでたどる。
     * @param byteCount Value列の後ろに置く、走査しない数値等のバイト数。
     * @param value 結果の参照。出力前の値は変更しない。以後の確保をまたいで使うならHeapRootを先に登録する。
     * @return 成功時true。型・サイズ・メモリーの失敗は実行エラーにする。
     * @details Runningで、未登録のC++一時参照がない位置からのみ呼ぶ。失敗時はGC後に一度だけ確保を再試行する。
     * 本体内Stringの文字列プールとの寿命統合、Pythonの本体型・生成命令は後続段階の対象。
     */
    bool allocateHeapValue(uint16_t classId, uint32_t valueCount, size_t byteCount, Value &value);
    /**
     * @brief 現在の根から同期GCを実行する。GC自身は本体を移動せず、追加領域も確保しない。
     * @return 実行中・一時停止・通常終了・エラー状態の有効な値領域で回収できたらtrue。
     * @details loop()の命令境界か、全一時参照をHeapRootで登録したC++処理だけが呼ぶ。
     * 関数終了は参照スロットを除くだけで本体を解放しない。停止中の診断値は根として保持する。
     */
    bool collectHeap();
    /**
     * @brief 借用参照・動的文字列・機器ハンドル・GC本体を解放する。数値と診断情報は保持する。
     */
    void stop();
    /**
     * @brief Stringを検査して長さ付きUTF-8を借用する。
     * @param value 対象。
     * @param data 出力先。
     * @param length バイト長。
     * @return 有効ならtrue。
     */
    bool stringBytes(const Value &value, const char *&data, size_t &length) const;
    /**
     * @brief 停止値と参照中の文字列を含むSnapshot長を計算する。
     * @return 不正状態・上限超過時0。
     */
    size_t debugSnapshotLength() const;
    /**
     * @brief 一時停止中の値を通信形式へコピーする。
     * @param data 出力。
     * @param length debugSnapshotLength()の値。
     * @return 成功時true。
     */
    bool copyDebugSnapshot(uint8_t *data, size_t length) const;
    /**
     * @brief 文・関数入口・暗黙returnのPCを検証し停止点bitsetへ全置換する。
     * @param points 昇順のPC。
     * @param count 個数。
     * @param code 検証済み命令列。
     * @param length 命令列長。
     * @return 成功時true。Runningでは変更不可。旧設定は失敗時も維持する。
     */
    bool configureDebugPoints(const uint16_t *points, uint32_t count, const uint8_t *code, uint32_t length);
    /**
     * @brief 接続・プログラム世代変更時に停止点と保留ステップを破棄する。loop()だけが呼ぶ。
     */
    void clearDebugPoints();
    /**
     * @brief Pausedから値・機器を保持して実行する。
     * @param action 実行方法。
     * @return 設定済みの有効な操作ならtrue。
     */
    bool resumeDebug(DebugAction action);

    private:
    /**
     * @brief VMの値スタック・大域・ネイティブ結果・C++側の根を列挙する。
     * @param heap 回収中のヒープ。
     * @param context BytecodeVMの借用ポインター。
     */
    static void traceHeapRoots(BytecodeHeap &heap, void *context);
    /**
     * @brief 本体先頭のValue列だけから有効な本体参照をたどる。
     * @param heap 回収中のヒープ。
     * @param block VMが初期化した本体。後続の数値バイト列は読まない。
     */
    static void traceHeapValues(BytecodeHeap &heap, BytecodeHeap::Block &block);
    /**
     * @brief 使用スロット順で文字列IDの初出か調べる。
     * @param index 大域、スタックの順の添字。
     * @return 初出Stringならtrue。
     */
    bool firstDebugString(uint32_t index) const;
    /**
     * @brief 呼出し元の命令範囲・一時値下限・戻り値幅。
     */
    struct Frame {
        uint32_t base; ///< 呼出し元の命令範囲の先頭PC。ret()でbase_へ復元する。
        uint32_t end; ///< 呼出し元の命令範囲の末尾直後のPC。ret()でend_へ復元する。
        uint32_t floor; ///< 呼出し元の一時スタック下限。ret()でfloor_へ復元する。
        uint32_t results; ///< 呼出し元関数の戻り値スロット数。ret()でresultSlots_へ復元する。
        uint32_t id; ///< 呼出し元フレームの識別子。再帰と同じ深さの別呼出しを区別する。
    };
    /**
     * @brief プール内の文字列。Valueは添字だけを保持する。
     */
    struct Text {
        std::unique_ptr<char[]> bytes; ///< この動的文字列が所有するバイト領域。末尾に補助NULを置く。
        size_t length = 0; ///< bytesのUTF-8バイト長。末尾の補助NULは含めない。
    };
    /**
     * @brief エラー状態へ移る。
     * @param code 理由。
     * @return false。
     */
    bool fail(Error code);
    /**
     * @brief 1命令を実行する。
     */
    void step();
    /**
     * @brief 値を積む。
     * @param value 値。
     * @return 成功時true。
     */
    bool push(const Value &value);
    /**
     * @brief 固定領域を侵さず値を取り出す。
     * @param value 出力先。
     * @return 成功時true。
     */
    bool pop(Value &value);
    /**
     * @brief 数値型か検査する。
     * @param value 値。
     * @return 数値ならtrue。
     */
    static bool numeric(const Value &value);
    /**
     * @brief 外部から返せるスロットか検査する。
     * @param value 値。
     * @return 有効ならtrue。
     */
    bool validValue(const Value &value) const;
    /**
     * @brief 条件値を変換する。
     * @param value 値。
     * @param result 真偽。
     * @return 成功時true。
     */
    bool truth(const Value &value, bool &result);
    /**
     * @brief 四則・比較を行う。
     * @param opcode 命令番号。
     */
    void calculate(uint8_t opcode);
    /**
     * @brief ユーザー関数フレームを作る。
     * @param entry 入口PC。
     */
    void call(uint16_t entry);
    /**
     * @brief 宣言された幅の戻り値を呼出し元へコピーする。
     */
    void ret();
    /**
     * @brief 登録表のスタブを同期実行する。
     * @param id 単一ネイティブID。
     */
    void callNative(uint16_t id);
    /**
     * @brief 到達不能文字列を回収して結果領域を確保する。
     * @param length バイト長。
     * @param value 出力値。
     * @param bytes 書込先。
     * @return 成功時true。
     */
    bool allocateText(size_t length, Value &value, char *&bytes);
    /**
     * @brief 数値を整形する。
     * @param format 定数文字列ID。
     */
    void formatValue(uint16_t format);
    /**
     * @brief スタック上の断片を連結する。
     * @param count 断片数。
     */
    void buildString(uint16_t count);

    const NativeApi *nativeApi_ = nullptr; ///< bindNativeApi()が登録する借用表。登録元が表・ハッシュ・contextを保持する。
    bool nativeActive_ = false; ///< 現在の実行でスタブを呼んだか。stop()がresetを一度呼んで解除する。
    bool yielded_ = false; ///< ネイティブ呼出し後はloop()へ戻す。tick()が更新する。
    bool nativePending_ = false; ///< 同じnative命令の再試行は新しい停止点到達ではない。
    bool skipDebugArrival_ = false; ///< 再開直後の1回の到達だけを消費する。
    DebugAction debugAction_ = DebugAction::Continue; ///< resumeDebug()で選択した実行方法。停止または解除でContinueへ戻す。
    std::unique_ptr<uint8_t[]> debugPoints_; ///< configureDebugPoints()が所有するステップ停止点の1PC=1bit表。
    uint32_t debugCodeLength_ = 0; ///< debugPoints_が対応する命令列のバイト長。再開時に現コードと照合する。
    uint32_t frameId_ = 1; ///< 現在のフレームの識別子。再帰や同じ深さの別呼出しを区別する。
    uint32_t nextFrameId_ = 1; ///< call()が増加させるフレーム識別子の採番状態。start()で初期化する。
    uint32_t debugFrameId_ = 0; ///< resumeDebug()時点のフレーム識別子。Overの停止判定に使う。
    uint32_t debugDepth_ = 0; ///< resumeDebug()時点の呼出し深さ。復帰とOutの停止判定に使う。
    uint8_t abiHash_[32]{}; ///< start()が保持するversion=3イメージの32バイトABI指紋。呼出し直前も照合する。
    const uint8_t *table_ = nullptr; ///< 借用イメージ内の関数表。stop()で参照を解除する。
    const uint8_t *code_ = nullptr; ///< 借用イメージ内の命令列。命令先頭のデバッグbit7以外は変更しない。
    const uint8_t *strings_ = nullptr; ///< 借用イメージ内の文字列定数表。各レコードはoffsetとlengthの8バイト。
    const uint8_t *stringData_ = nullptr; ///< 借用イメージ内のUTF-8定数データ。長さで参照しNUL終端を要求しない。
    uint16_t stringCount_ = 0; ///< 借用イメージの文字列定数数。Stringの定数IDの上限判定に使う。
    uint32_t stringDataLength_ = 0; ///< UTF-8定数データ全体のバイト数。stringBytes()が参照範囲を検査する。
    uint32_t codeLength_ = 0; ///< 命令列全体のバイト長。start()で最大65536バイトに制限する。
    uint16_t functionCount_ = 0; ///< 関数表の件数。call()が入口PCを二分探索するときに使う。
    std::unique_ptr<Value[]> nativeResults_; ///< start()で最大結果幅を確保し、callNative()が初期化・再利用する。
    uint32_t nativeResultSlots_ = 0; ///< 再利用領域のスロット数。stop()で0へ戻す。
    uint32_t nativeRootSlots_ = 0; ///< 同期ネイティブ処理中だけ根として走査する結果スロット数。
    HeapRoot *heapRoots_ = nullptr; ///< C++の一時・保持値を根へ登録するガードの一覧。
    BytecodeHeap heap_{
        heapStatistics, MaxHeapBytes, HeapCollectionBytes}; ///< VMが所有するGC本体。heapStatisticsより先に破棄する。
    std::unique_ptr<Value[]> stack_; ///< start()が確保する値スタックの所有領域。stop()が解放する。
    Text texts_[MaxStrings]; ///< 動的文字列の所有プール。allocateText()が生存値から未参照領域を回収する。
    Frame frames_[MaxFrames]{}; ///< 呼出し元の実行範囲と戻り値幅を保存する配列。call()/ret()が更新する。
    uint32_t depth_ = 0; ///< frames_の使用件数。0はモジュール本体。
    uint32_t base_ = 0; ///< 現在の実行範囲の先頭PC。関数内分岐の相対PCの基準。
    uint32_t end_ = 0; ///< 現在の実行範囲の末尾直後のPC。step()が範囲外の実行を防ぐ。
    uint32_t floor_ = 0; ///< 現在の一時値スタックの下限。pop()が固定フレーム領域を保護する。
    uint32_t resultSlots_ = 1; ///< 現在の関数が返すスロット数。ret()が一時値の幅と一致するか検査する。
};
