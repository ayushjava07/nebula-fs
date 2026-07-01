#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_set>

namespace nebula {
namespace utils {

/// A simple memory allocator with object pooling.
///
/// Provides fast allocation and deallocation for fixed-size objects.
/// Maintains a free list for reuse and tracks allocated blocks.
class Allocator {
public:
    explicit Allocator(size_t blockSize = 64, size_t poolSize = 1024);
    ~Allocator() noexcept;

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
    Allocator(Allocator&&) noexcept = delete;
    Allocator& operator=(Allocator&&) noexcept = delete;

    /// Allocate a block of memory.
    void* allocate();

    /// Deallocate a previously allocated block.
    /// BUG #11: Does not remove from tracking, enabling double-free.
    void deallocate(void* ptr);

    /// Allocate and construct an object.
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate();
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// Destroy and deallocate an object.
    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    /// Get the block size.
    [[nodiscard]] size_t blockSize() const noexcept { return blockSize_; }

    /// Get the number of free blocks.
    [[nodiscard]] size_t freeCount() const noexcept { return freeList_.size(); }

    /// Get the total number of allocated blocks.
    [[nodiscard]] size_t totalAllocated() const noexcept { return allocated_.size(); }

    /// Reset the allocator, freeing all memory.
    void reset();

private:
    size_t blockSize_;
    size_t poolSize_;

    struct Pool {
        std::vector<uint8_t> memory;
        std::vector<void*> blocks;
    };

    std::vector<std::unique_ptr<Pool>> pools_;
    std::vector<void*> freeList_;
    std::unordered_set<void*> allocated_;
    std::mutex mutex_;

    Pool* createPool();
};

/// Object pool that returns raw pointers.
/// BUG #17: Objects can be freed while the pool still holds references.
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialCapacity = 1024)
        : allocator_(sizeof(T), initialCapacity) {}

    template<typename... Args>
    T* acquire(Args&&... args) {
        return allocator_.construct<T>(std::forward<Args>(args)...);
    }

    void release(T* obj) {
        allocator_.destroy(obj);
        // BUG #17: obj is freed but if anyone still holds the raw pointer
        // returned by acquire(), it's now a dangling pointer.
    }

    size_t available() const noexcept { return allocator_.freeCount(); }

private:
    Allocator allocator_;
};

} // namespace utils
} // namespace nebula
