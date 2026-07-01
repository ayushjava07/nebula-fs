#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace nebula {
namespace storage {

struct CacheEntry {
    uint8_t* data;
    size_t size;
};

class CacheManager {
public:
    CacheManager() = default;
    ~CacheManager();

    void put(uint64_t key, const uint8_t* data, size_t size);
    CacheEntry* get(uint64_t key);
    void evict(uint64_t key);

private:
    std::unordered_map<uint64_t, CacheEntry*> cache_;
};

} // namespace storage
} // namespace nebula
