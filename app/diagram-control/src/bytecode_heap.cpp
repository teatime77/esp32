/**
 * @file bytecode_heap.cpp
 * @brief 本体の確保とスイープ。マークはVMが根と参照列を直接たどる。
 */
#include "bytecode_heap.h"
#include <cstdlib>
#include <cstring>
#include <new>

/**
 * @copydoc BytecodeHeap::BytecodeHeap
 */
BytecodeHeap::BytecodeHeap(size_t limitBytes) : limitBytes_(limitBytes) {
}

/**
 * @copydoc BytecodeHeap::~BytecodeHeap
 */
BytecodeHeap::~BytecodeHeap() {
    clear();
}

/**
 * @copydoc BytecodeHeap::allocate
 */
BytecodeHeap::Block *BytecodeHeap::allocate(size_t bytes) {
    if (bytes > SIZE_MAX - sizeof(Block)) {
        return nullptr;
    }
    size_t total = sizeof(Block) + bytes;
    if (total > limitBytes_) {
        return nullptr;
    }
    for (const Block *block = blocks; block; block = block->next) {
        const size_t allocated = sizeof(Block) + block->bytes;
        if (allocated > limitBytes_ - total) {
            return nullptr;
        }
        total += allocated;
    }
    void *allocation = std::malloc(sizeof(Block) + bytes);
    if (!allocation) {
        return nullptr;
    }
    Block *block = new (allocation) Block{};
    block->data = block + 1;
    block->bytes = bytes;
    block->next = blocks;
    std::memset(block->data, 0, bytes);
    blocks = block;
    return block;
}

/**
 * @copydoc BytecodeHeap::contains
 */
bool BytecodeHeap::contains(const Block *block) const {
    for (const Block *current = blocks; current; current = current->next) {
        if (current == block) {
            return true;
        }
    }
    return false;
}

/**
 * @copydoc BytecodeHeap::sweep
 */
void BytecodeHeap::sweep() {
    Block **link = &blocks;
    while (*link) {
        Block *block = *link;
        if (block->marked) {
            link = &block->next;
        }
        else {
            *link = block->next;
            block->~Block();
            std::free(block);
        }
    }
}

/**
 * @copydoc BytecodeHeap::clear
 */
void BytecodeHeap::clear() {
    while (blocks) {
        Block *block = blocks;
        blocks = block->next;
        block->~Block();
        std::free(block);
    }
}
