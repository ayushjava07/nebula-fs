#include "nebula/storage/CacheManager.hpp"
#include <cstring>
#include <algorithm>

namespace nebula {
namespace storage {

CacheManager::~CacheManager() {
    for (auto& [key, entry] : cache_) {
        delete[] entry->data;
        delete entry;
    }
    cache_.clear();
}

void CacheManager::put(uint64_t key, const uint8_t* data, size_t size) {
    // Bug #9: If key already exists, the old entry's data is leaked
    auto* entry = new CacheEntry;
    entry->size = size;
    entry->data = new uint8_t[size];
    std::memcpy(entry->data, data, size);
    cache_[key] = entry;
}

CacheEntry* CacheManager::get(uint64_t key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return nullptr;
    // Bug #18: No synchronization - another thread could evict
    //         this entry between find and dereference
    return it->second;
}

void CacheManager::evict(uint64_t key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return;

    // Bug #9: Frees the data buffer but leaves the CacheEntry pointer
    //         dangling if another caller already holds it via get()
    delete[] it->second->data;
    // Bug #9+18: The CacheEntry itself and the map entry are NOT cleaned up,
    //         so concurrent get() returns a CacheEntry with freed data
    it->second->data = nullptr; // only nulls data, entry still in map
    // BUG: it->second is still in cache_, but its data is freed
}

} // namespace storage
} // namespace nebula
