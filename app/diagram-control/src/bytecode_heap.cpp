/**
 * @file bytecode_heap.cpp
 * @brief BytecodeVM専用のヒープ確保と非再帰マーク＆スイープ。
 */
#include "bytecode_heap.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <esp_timer.h>

/**
 * @brief 確保上限と診断の出力先を保持する。
 * @param statistics VMが所有する診断。
 * @param limitBytes ヘッダー込みの最大確保量。
 * @param collectionBytes GC要求の累積確保量。
 */
BytecodeHeap::BytecodeHeap(Statistics &statistics, size_t limitBytes, size_t collectionBytes)
    : statistics_(statistics), limitBytes_(limitBytes), collectionBytes_(collectionBytes) {
}

/**
 * @brief 残存本体を解放する。診断の出力先はまだ生存している必要がある。
 */
BytecodeHeap::~BytecodeHeap() {
    clear();
}

/**
 * @brief 本体を移動しない個別領域として確保する。
 * @param bytes 本体バイト数。
 * @param trace 本体の参照走査。nullは参照なし。
 * @return 成功時のヘッダー。失敗時nullで既存本体と診断は変更しない。
 */
BytecodeHeap::Block *BytecodeHeap::allocate(size_t bytes, Trace trace) {
    if (collecting_ || nextId_ == UINT32_MAX || bytes > SIZE_MAX - sizeof(Block)) {
        return nullptr;
    }
    const size_t total = sizeof(Block) + bytes;
    if (total > limitBytes_ || statistics_.bytes > limitBytes_ - total) {
        return nullptr;
    }
    void *allocation = std::malloc(total);
    if (!allocation) {
        return nullptr;
    }
    Block *block = new (allocation) Block{};
    block->id = ++nextId_;
    block->data = block + 1;
    block->bytes = bytes;
    block->trace = trace;
    block->next = blocks_;
    std::memset(block->data, 0, bytes);
    blocks_ = block;
    statistics_.bytes += total;
    ++statistics_.objects;
    if (statistics_.bytes > statistics_.peakBytes) {
        statistics_.peakBytes = statistics_.bytes;
    }
    // GCまで領域を解放しないため、累積値もlimitBytes_を超えず加算はoverflowしない。
    sinceCollection_ += total;
    return block;
}

/**
 * @brief 本体の所属を、未検証ポインターを逆参照せずに検査する。
 * @param block 対象のヘッダー。
 * @return 現在の確保一覧に存在すればtrue。
 */
bool BytecodeHeap::contains(const Block *block) const {
    for (const Block *current = blocks_; current; current = current->next) {
        if (current == block) {
            return true;
        }
    }
    return false;
}

/**
 * @copydoc BytecodeHeap::visit
 */
void BytecodeHeap::visit(Visitor visitor, void *context) const {
    if (visitor) {
        for (const Block *block = blocks_; block; block = block->next) {
            visitor(*block, context);
        }
    }
}

/**
 * @brief 初回到達の本体だけを探索待ちへ追加する。
 * @param block このヒープの有効なヘッダーまたはnull。
 */
void BytecodeHeap::mark(Block *block) {
    if (!collecting_ || !block || block->marked) {
        return;
    }
    block->marked = true;
    block->pending = pending_;
    pending_ = block;
}

/**
 * @brief 登録済みの根から本体をマークし、未到達の本体を回収する。
 * @param roots VMが提供する根の列挙。
 * @param context VMの借用ポインター。
 * @return 再入せず回収を完了したらtrue。
 */
bool BytecodeHeap::collect(Roots roots, void *context) {
    if (collecting_) {
        return false;
    }
    const int64_t started = esp_timer_get_time();
    collecting_ = true;
    pending_ = nullptr;
    for (Block *block = blocks_; block; block = block->next) {
        block->marked = false;
        block->pending = nullptr;
    }
    if (roots) {
        roots(*this, context);
    }
    while (pending_) {
        Block *block = pending_;
        pending_ = block->pending;
        block->pending = nullptr;
        if (block->trace) {
            block->trace(*this, *block);
        }
    }
    statistics_.lastFreedBytes = 0;
    statistics_.lastFreedObjects = 0;
    Block **link = &blocks_;
    while (*link) {
        Block *block = *link;
        if (block->marked) {
            link = &block->next;
        }
        else {
            *link = block->next;
            const size_t total = sizeof(Block) + block->bytes;
            statistics_.bytes -= total;
            --statistics_.objects;
            statistics_.lastFreedBytes += total;
            ++statistics_.lastFreedObjects;
            block->~Block();
            std::free(block);
        }
    }
    sinceCollection_ = 0;
    collecting_ = false;
    ++statistics_.collections;
    statistics_.lastMicroseconds = uint64_t(esp_timer_get_time() - started);
    if (statistics_.lastMicroseconds > statistics_.maxMicroseconds) {
        statistics_.maxMicroseconds = statistics_.lastMicroseconds;
    }
    return true;
}

/**
 * @brief 次の安全な命令境界でGCが必要か判定する。
 * @return 回収後の確保量が設定値以上ならtrue。設定値0は自動要求なし。
 */
bool BytecodeHeap::collectionRequested() const {
    return collectionBytes_ != 0 && sinceCollection_ >= collectionBytes_;
}

/**
 * @brief 本体をすべて解放し、現在の確保量だけを0へ戻す。
 */
void BytecodeHeap::clear() {
    while (blocks_) {
        Block *block = blocks_;
        blocks_ = block->next;
        block->~Block();
        std::free(block);
    }
    pending_ = nullptr;
    sinceCollection_ = 0;
    statistics_.bytes = 0;
    statistics_.objects = 0;
    nextId_ = 0;
}
