/**
 * @file bytecode_heap.h
 * @brief BytecodeVMが使う、移動しないマーク＆スイープヒープの契約。
 * @details 根の列挙とValueの走査はbytecode_vm.cpp、本体の確保・探索・回収はbytecode_heap.cppが担当する。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief loop()から同期的に使用するGC管理領域。参照カウント・所有フレーム・finalizerは持たない。
 */
class BytecodeHeap {
    public:
    /**
     * @brief 走査関数が参照する本体ヘッダーの前方宣言。
     */
    struct Block;
    /**
     * @brief 本体内の有効な参照だけをmark()へ渡す走査関数。
     * @param heap 回収中のヒープ。
     * @param block 走査対象。確保・解放やVMの再入を行わない。
     */
    using Trace = void (*)(BytecodeHeap &heap, Block &block);
    /**
     * @brief VMとC++側が保持する有効な根をmark()へ渡す関数。
     * @param heap 回収中のヒープ。
     * @param context 呼出し元の借用状態。
     */
    using Roots = void (*)(BytecodeHeap &heap, void *context);

    /**
     * @brief 本体の直前に置く管理情報。dataはmax_align_tに整列し、生存中は移動しない。
     * @details VMだけがvalueCount・classIdとdataの内容を設定する。リンク・マーク状態はヒープだけが更新する。
     */
    struct alignas(max_align_t) Block {
        void *data; ///< このヘッダーの直後にある、ゼロ初期化した本体。
        size_t bytes; ///< ヘッダーを除く本体のバイト長。
        uint32_t valueCount = 0; ///< 本体先頭に配置するVMのValueの数。後続の数値バイト列は走査しない。
        uint16_t classId = 0; ///< VMが設定する非0の本体型ID。機器ハンドルとは別の参照種別で扱う。
        Trace trace; ///< 本体の参照走査。数値だけの本体ではnull。
        Block *next; ///< 全確保領域の単方向リスト。共有参照数の管理表ではない。
        Block *pending = nullptr; ///< GC中だけ使う探索待ちリスト。
        bool marked = false; ///< 今回のGCで到達済みか。
    };

    /**
     * @brief VMが公開する実行診断。バイト数は管理ヘッダー込みで、malloc内部の管理量は含まない。
     */
    struct Statistics {
        size_t bytes = 0; ///< 現在の確保量。確保・回収・clearで更新する。
        size_t peakBytes = 0; ///< 実行開始後の最大確保量。
        size_t objects = 0; ///< 現在の確保領域数。
        size_t lastFreedBytes = 0; ///< 直前のGCが解放したバイト数。
        size_t lastFreedObjects = 0; ///< 直前のGCが解放した領域数。
        uint32_t collections = 0; ///< 完了したGCの回数。
        uint64_t lastMicroseconds = 0; ///< 直前のGCの経過時間。単位はマイクロ秒。
        uint64_t maxMicroseconds = 0; ///< GCの最大経過時間。60fpsに収まる保証値ではない。
    };

    /**
     * @brief 診断の出力先と実行上限を設定する。ここでは本体を確保しない。
     * @param statistics ヒープより長く生存する診断の出力先。
     * @param limitBytes 管理ヘッダーを含む最大確保量。
     * @param collectionBytes GCを要求する、前回回収後の累積確保量。
     */
    BytecodeHeap(Statistics &statistics, size_t limitBytes, size_t collectionBytes);
    /**
     * @brief 残っている全本体を解放する。
     */
    ~BytecodeHeap();
    /**
     * @brief 所有する本体と診断の参照を複製しない。
     * @param other コピー元。
     */
    BytecodeHeap(const BytecodeHeap &other) = delete;
    /**
     * @brief 所有する本体と診断の参照をコピー代入しない。
     * @param other コピー元。
     * @return 使用禁止。
     */
    BytecodeHeap &operator=(const BytecodeHeap &other) = delete;
    /**
     * @brief ゼロ初期化した本体とヘッダーをまとめて確保する。内部ではGCを起動しない。
     * @param bytes 本体のバイト長。0も許可する。
     * @param trace 参照の走査関数。数値だけの本体はnull。
     * @return 確保した領域。上限・サイズ超過、確保失敗、GC中はnull。
     */
    Block *allocate(size_t bytes, Trace trace);
    /**
     * @brief ヒープが現在所有する先頭ポインターか検査する。対象のポインター自体は逆参照しない。
     * @param block 検査するポインター。
     * @return 所有中ならtrue。null・内部アドレス・別ヒープの領域はfalse。
     */
    bool contains(const Block *block) const;
    /**
     * @brief 根の列挙と本体走査を行い、到達不能な領域を解放する。
     * @param roots 根の列挙関数。nullなら根はない。
     * @param context 根の列挙関数へ渡す状態。
     * @return GCを完了したらtrue。再入時はfalse。
     * @details 全VM実行を止めて同期実行する。GC中にmallocや再帰を使わず、各本体のpendingを作業領域にする。
     * 数値配列はデータ全体を読まず、走査関数が列挙した参照だけをたどる。循環はmarkedで打ち切る。
     */
    bool collect(Roots roots, void *context);
    /**
     * @brief 有効な本体を到達済みにし、初回だけ探索待ちへ追加する。
     * @param block このヒープが発行し、現在も生存している領域。nullは無視する。
     * @details 根・走査関数からだけ呼ぶ。O(1)で処理するため所属表の検索は行わない。
     * C++からの値の受入れはVM側でcontains()を使って検証する。任意の整数や解放済み参照を渡してはならない。
     */
    void mark(Block *block);
    /**
     * @brief 累積確保量が回収開始の目安に達したか調べる。
     * @return 次の安全な命令境界で回収すべきならtrue。
     */
    bool collectionRequested() const;
    /**
     * @brief 全本体を無条件に解放する。ピーク・GC時間等の診断は保持する。
     * @details GC中は呼ばない。VMは先に値とC++側の根を失効させる。
     */
    void clear();

    private:
    Statistics &statistics_; ///< VMが所有する診断。更新はこのクラスだけが行う。
    const size_t limitBytes_; ///< 管理ヘッダー込みの実行上限。
    const size_t collectionBytes_; ///< 安全な位置でGCを要求する累積確保量。
    size_t sinceCollection_ = 0; ///< 前回GC以降に確保したバイト数。
    Block *blocks_ = nullptr; ///< 全確保領域の先頭。
    Block *pending_ = nullptr; ///< GCの探索待ちリストの先頭。
    bool collecting_ = false; ///< GCの再入・GC中の確保を禁止する状態。
};
