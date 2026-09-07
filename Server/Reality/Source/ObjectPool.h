#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>
#include "Common.h"

// Thread-safe generic Object Pool to prevent heap fragmentation
template<typename T>
class ObjectPool {
private:
    struct PoolChunk {
        uint8* memory;
        size_t capacity;
        size_t used;
        PoolChunk(size_t cap) : capacity(cap), used(0) {
            memory = new uint8[cap * sizeof(T)];
        }
        ~PoolChunk() {
            delete[] memory;
        }
    };

    std::vector<PoolChunk*> m_chunks;
    std::vector<T*> m_freeList;
    std::mutex m_mutex;
    size_t m_chunkSize;

    void allocateChunk() {
        PoolChunk* chunk = new PoolChunk(m_chunkSize);
        m_chunks.push_back(chunk);
        for (size_t i = 0; i < m_chunkSize; ++i) {
            T* ptr = reinterpret_cast<T*>(chunk->memory + (i * sizeof(T)));
            m_freeList.push_back(ptr);
        }
    }

public:
    ObjectPool(size_t chunkSize = 1024) : m_chunkSize(chunkSize) {
        allocateChunk();
    }

    ~ObjectPool() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (PoolChunk* chunk : m_chunks) {
            delete chunk;
        }
        m_chunks.clear();
        m_freeList.clear();
    }

    // Acquire memory for an object (does not call constructor)
    template<typename... Args>
    T* acquire(Args&&... args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_freeList.empty()) {
            allocateChunk();
        }
        T* ptr = m_freeList.back();
        m_freeList.pop_back();
        
        // Placement new to call constructor with exception safety
        try {
            return new(ptr) T(std::forward<Args>(args)...);
        } catch (...) {
            m_freeList.push_back(ptr);
            throw;
        }
    }

    // Release memory back to pool (calls destructor explicitly)
    void release(T* ptr) {
        if (!ptr) return;
        
        // Explicitly call destructor
        ptr->~T();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_freeList.push_back(ptr);
    }
};
