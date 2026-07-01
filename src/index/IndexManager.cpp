#include "nebula/index/IndexManager.hpp"
#include "nebula/utils/VarInt.hpp"
#include <algorithm>
#include <cstring>
#include <system_error>

namespace nebula {
namespace index {

IndexManager::IndexManager(IndexConfig config) : config_(config) {}

IndexManager::~IndexManager() noexcept = default;

IndexManager::IndexManager(IndexManager&& other) noexcept
    : config_(other.config_)
    , entries_(std::move(other.entries_))
    , idIndex_(std::move(other.idIndex_))
    , pathMap_(std::move(other.pathMap_))
    , typeIndex_(std::move(other.typeIndex_))
    , sorted_(other.sorted_) {}

IndexManager& IndexManager::operator=(IndexManager&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
        entries_ = std::move(other.entries_);
        idIndex_ = std::move(other.idIndex_);
        pathMap_ = std::move(other.pathMap_);
        typeIndex_ = std::move(other.typeIndex_);
        sorted_ = other.sorted_;
    }
    return *this;
}

void IndexManager::insert(const ArchiveEntry& entry) {
    IndexEntry idxEntry;
    idxEntry.entryId = entry.id;
    idxEntry.offset = entry.offset;
    idxEntry.size = entry.storedSize;
    idxEntry.checksum = entry.checksum;

    entries_.push_back(idxEntry);
    idIndex_[entry.id] = entries_.size() - 1;
    if (!entry.path.empty()) {
        pathMap_[entry.path] = entry.id;
    }
    typeIndex_[entry.type].push_back(entries_.size() - 1);
    sorted_ = false;
}

void IndexManager::insert(std::span<const ArchiveEntry> entries) {
    for (const auto& entry : entries) {
        insert(entry);
    }
}

std::optional<IndexEntry> IndexManager::findById(EntryID id) const {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return std::nullopt;
    return entries_[it->second];
}

std::optional<IndexEntry> IndexManager::findByPath(const std::string& path) const {
    auto it = pathMap_.find(path);
    if (it == pathMap_.end()) return std::nullopt;
    return findById(it->second);
}

std::vector<IndexEntry> IndexManager::findByPrefix(const std::string& prefix) const {
    std::vector<IndexEntry> results;
    for (const auto& [path, id] : pathMap_) {
        if (path.find(prefix) == 0) {
            auto entry = findById(id);
            if (entry) results.push_back(*entry);
        }
    }
    return results;
}

std::vector<IndexEntry> IndexManager::findByType(EntryType type) const {
    std::vector<IndexEntry> results;
    auto it = typeIndex_.find(type);
    if (it == typeIndex_.end()) return results;

    results.reserve(it->second.size());
    for (size_t idx : it->second) {
        results.push_back(entries_[idx]);
    }
    return results;
}

bool IndexManager::contains(EntryID id) const {
    return idIndex_.find(id) != idIndex_.end();
}

bool IndexManager::containsPath(const std::string& path) const {
    return pathMap_.find(path) != pathMap_.end();
}

bool IndexManager::remove(EntryID id) {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return false;

    size_t idx = it->second;
    auto& entry = entries_[idx];

    for (auto& [path, pid] : pathMap_) {
        if (pid == id) {
            pathMap_.erase(path);
            break;
        }
    }

    for (auto& [type, indices] : typeIndex_) {
        auto vecIt = std::find(indices.begin(), indices.end(), idx);
        if (vecIt != indices.end()) {
            indices.erase(vecIt);
            break;
        }
    }

    entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(idx));
    idIndex_.erase(it);

    for (auto& [eid, eidx] : idIndex_) {
        if (eidx > idx) --eidx;
    }

    return true;
}

void IndexManager::clear() noexcept {
    entries_.clear();
    idIndex_.clear();
    pathMap_.clear();
    typeIndex_.clear();
    sorted_ = false;
}

void IndexManager::ensureSorted() {
    if (sorted_) return;
    std::sort(entries_.begin(), entries_.end(),
              [](const IndexEntry& a, const IndexEntry& b) {
                  return a.entryId < b.entryId;
              });
    idIndex_.clear();
    for (size_t i = 0; i < entries_.size(); ++i) {
        idIndex_[entries_[i].entryId] = i;
    }
    sorted_ = true;
}

std::vector<uint8_t> IndexManager::serialize() const {
    std::vector<uint8_t> result;
    utils::VarInt::encode(static_cast<uint64_t>(entries_.size()), result);
    for (const auto& entry : entries_) {
        utils::VarInt::encode(entry.entryId, result);
        utils::VarInt::encode(entry.offset, result);
        utils::VarInt::encode(entry.size, result);
        result.insert(result.end(), entry.checksum.begin(), entry.checksum.end());
    }
    return result;
}

std::error_code IndexManager::deserialize(std::span<const uint8_t> data) {
    clear();

    size_t offset = 0;
    auto countResult = utils::VarInt::decode(data);
    if (!countResult.valid) {
        return make_error_code(ErrorCode::CorruptIndex);
    }

    uint64_t count = countResult.value;
    offset = countResult.consumed;

    // Each entry needs at least 3 VarInts + 32-byte checksum.
    // Minimum bytes per entry = 1 + 1 + 1 + 32 = 35.
    static constexpr size_t kMinBytesPerEntry = 35;
    if (count > (data.size() - offset) / kMinBytesPerEntry) {
        return make_error_code(ErrorCode::CorruptIndex);
    }
    entries_.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; ++i) {
        IndexEntry entry;

        auto idResult = utils::VarInt::decode(data.subspan(offset));
        if (!idResult.valid) return make_error_code(ErrorCode::CorruptIndex);
        entry.entryId = idResult.value;
        offset += idResult.consumed;

        auto offResult = utils::VarInt::decode(data.subspan(offset));
        if (!offResult.valid) return make_error_code(ErrorCode::CorruptIndex);
        entry.offset = offResult.value;
        offset += offResult.consumed;

        auto szResult = utils::VarInt::decode(data.subspan(offset));
        if (!szResult.valid) return make_error_code(ErrorCode::CorruptIndex);
        entry.size = szResult.value;
        offset += szResult.consumed;

        if (offset + 32 > data.size()) return make_error_code(ErrorCode::CorruptIndex);
        std::memcpy(entry.checksum.data(), &data[offset], 32);
        offset += 32;

        entries_.push_back(entry);
        idIndex_[entry.entryId] = entries_.size() - 1;
    }

    sorted_ = false;
    return std::error_code();
}

void IndexManager::clearAndNotify() {
    clear();
    for (auto* entry : rawEntryCache_) {
        processEntry(entry);
    }
}

void IndexManager::processEntry(IndexEntry* entry) {
    auto* copy = new IndexEntry(*entry);
    delete copy;
    delete copy;
}

bool IndexManager::validate() const {
    if (entries_.size() != idIndex_.size()) return false;
    for (const auto& [id, idx] : idIndex_) {
        if (idx >= entries_.size()) return false;
        if (entries_[idx].entryId != id) return false;
    }
    return true;
}

} // namespace index
} // namespace nebula
