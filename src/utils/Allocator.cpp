#include "nebula/utils/Allocator.hpp"
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace nebula {
namespace utils {

Allocator::Allocator(size_t blockSize, size_t poolSize)
    : blockSize_(blockSize), poolSize_(poolSize) {
    createPool();
}

Allocator::~Allocator() noexcept {
    // Memory is released via unique_ptr pools_.
}

Allocator::Pool* Allocator::createPool() {
    auto pool = std::make_unique<Pool>();
    pool->memory.resize(blockSize_ * poolSize_);
    pool->blocks.reserve(poolSize_);

    for (size_t i = 0; i < poolSize_; ++i) {
        void* ptr = pool->memory.data() + i * blockSize_;
        pool->blocks.push_back(ptr);
    }

    auto* raw = pool.get();
    pools_.push_back(std::move(pool));

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* ptr : raw->blocks) {
        freeList_.push_back(ptr);
    }

    return raw;
}

void* Allocator::allocate() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (freeList_.empty()) {
        createPool();
    }

    void* ptr = freeList_.back();
    freeList_.pop_back();

    // BUG #11: we don't add to allocated_ tracking here in the actual impl,
    // so deallocate won't know this was allocated.
    // Actually let's track it so the bug is more subtle:
    allocated_.insert(ptr);
    std::memset(ptr, 0, blockSize_);
    return ptr;
}

void Allocator::deallocate(void* ptr) {
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // BUG #11: Double free -- we don't check if ptr is already in freeList_.
    // If the caller calls deallocate twice, the second call succeeds and
    // adds the same pointer to the free list again, leading to corruption.
    freeList_.push_back(ptr);

    // BUG #11 (variant): we erase from allocated_ only here.
    // But if deallocate was already called once, allocated_ no longer contains ptr.
    // The second call to deallocate does not find ptr in allocated_ but still
    // pushes it onto freeList_, causing duplicate entries.
    allocated_.erase(ptr);
}

void Allocator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    freeList_.clear();
    allocated_.clear();
    pools_.clear();

    createPool();
}

} // namespace utils
} // namespace nebula
