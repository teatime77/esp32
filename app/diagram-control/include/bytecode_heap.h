/**
 * @file bytecode_heap.h
 * @brief 全メモリーブロックの確保・スイープ。根と再帰マークはbytecode_vm.cppが担当する。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief loop()とVMだけが同期的に操作する、移動しない本体の管理。
 */
class BytecodeHeap {
    public:
    /**
     * @brief 本体の管理情報。VMが参照列・型・配列種別を設定する。
     */
    struct alignas(max_align_t) Block {
        void *data; ///< ヘッダー直後の整列した本体。
        size_t bytes; ///< 本体バイト数。確保上限と配置検査に使用する。
        uint32_t valueCount = 0; ///< 先頭にあるValue数。参照探索の範囲。
        uint16_t classId = 0; ///< VMの本体型ID。
        uint8_t arrayKind = 0; ///< 0=通常、1=float32、2=uint8、3=object配列。
        Block *next; ///< 全ブロック一覧の次。
        bool marked = false; ///< VMの再帰マークで到達済みか。
    };

    Block *blocks = nullptr; ///< 全ブロック一覧。外部は参照のみ。allocate・sweep・clearが更新する。

    /**
     * @brief 確保上限を設定する。
     * @param limitBytes ヘッダー込みの合計上限。
     */
    explicit BytecodeHeap(size_t limitBytes);
    /**
     * @brief 全ブロックを解放する。
     */
    ~BytecodeHeap();
    /**
     * @brief 所有ブロックのコピーを禁止する。
     * @param other コピー元。
     */
    BytecodeHeap(const BytecodeHeap &other) = delete;
    /**
     * @brief 所有ブロックのコピー代入を禁止する。
     * @param other コピー元。
     * @return 使用禁止。
     */
    BytecodeHeap &operator=(const BytecodeHeap &other) = delete;
    /**
     * @brief 本体をゼロ初期化して確保する。GCや再試行を行わない。
     * @param bytes 本体のバイト数。
     * @return 成功時のブロック。上限・サイズ超過またはmalloc失敗ならnull。
     */
    Block *allocate(size_t bytes);
    /**
     * @brief 未検証ポインターを逆参照せず所属を検査する。
     * @param block 対象。
     * @return 現在の一覧に含まれればtrue。
     */
    bool contains(const Block *block) const;
    /**
     * @brief マークされなかった本体を一覧から除き解放する。
     * @details VMが根から再帰マークした後だけ呼ぶ。
     */
    void sweep();
    /**
     * @brief 全本体を解放する。
     */
    void clear();

    private:
    const size_t limitBytes_; ///< 合計確保量の上限。使用量は確保時に一覧から求める。
};
