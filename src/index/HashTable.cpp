#include "nebula/index/HashTable.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>
#include <algorithm>

namespace nebula {
namespace index {

HashTable::HashTable(size_t initialCapacity) {
    entries_.resize(std::max(initialCapacity, static_cast<size_t>(16)));
}

HashTable::HashTable(HashTable&& other) noexcept
    : entries_(std::move(other.entries_))
    , size_(other.size_) {}

HashTable& HashTable::operator=(HashTable&& other) noexcept {
    if (this != &other) {
        entries_ = std::move(other.entries_);
        size_ = other.size_;
    }
    return *this;
}

uint64_t HashTable::hashKey(const std::string& key) const noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : key) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

size_t HashTable::idealSlot(uint64_t hash) const noexcept {
    return static_cast<size_t>(hash & (entries_.size() - 1));
}

void HashTable::insert(const std::string& key, EntryID value) {
    growIfNeeded();

    auto fullHash = hashKey(key);
    auto slot = idealSlot(fullHash);

    Entry newEntry;
    newEntry.hash = fullHash;
    newEntry.entryId = value;
    newEntry.probeLength = 0;
    newEntry.occupied = true;
    newEntry.tombstone = false;

    while (true) {
        auto& current = entries_[slot];
        if (!current.occupied || current.tombstone) {
            current = newEntry;
            ++size_;
            return;
        }

        if (current.hash == fullHash) {
            current.entryId = value;
            return;
        }

        if (newEntry.probeLength > current.probeLength) {
            std::swap(newEntry, current);
            ++newEntry.probeLength;
        }

        ++newEntry.probeLength;
        slot = (slot + 1) & (entries_.size() - 1);
    }
}

std::optional<EntryID> HashTable::find(const std::string& key) const {
    auto fullHash = hashKey(key);
    auto slot = idealSlot(fullHash);
    size_t probeLength = 0;

    while (probeLength <= entries_[slot].probeLength) {
        const auto& entry = entries_[slot];
        if (entry.occupied && !entry.tombstone && entry.hash == fullHash) {
            return entry.entryId;
        }
        if (!entry.occupied && !entry.tombstone) {
            return std::nullopt;
        }
        ++probeLength;
        slot = (slot + 1) & (entries_.size() - 1);
    }

    return std::nullopt;
}

bool HashTable::contains(const std::string& key) const {
    return find(key).has_value();
}

bool HashTable::remove(const std::string& key) {
    auto fullHash = hashKey(key);
    auto slot = idealSlot(fullHash);
    size_t probeLength = 0;

    while (probeLength <= entries_[slot].probeLength) {
        auto& entry = entries_[slot];
        if (entry.occupied && !entry.tombstone && entry.hash == fullHash) {
            entry.tombstone = true;
            entry.occupied = false;
            --size_;
            return true;
        }
        if (!entry.occupied && !entry.tombstone) {
            return false;
        }
        ++probeLength;
        slot = (slot + 1) & (entries_.size() - 1);
    }

    return false;
}

double HashTable::loadFactor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(entries_.size());
}

void HashTable::clear() {
    for (auto& entry : entries_) {
        entry.occupied = false;
        entry.tombstone = false;
        entry.probeLength = 0;
    }
    size_ = 0;
}

void HashTable::reserve(size_t newCapacity) {
    if (newCapacity > entries_.size()) {
        rehash(newCapacity);
    }
}

void HashTable::growIfNeeded() {
    if (loadFactor() > 0.7) {
        rehash(entries_.size() * 2);
    }
}

void HashTable::rehash(size_t newCapacity) {
    auto oldEntries = std::move(entries_);
    entries_.clear();
    entries_.resize(newCapacity);
    size_ = 0;

    for (auto& entry : oldEntries) {
        if (entry.occupied && !entry.tombstone) {
            auto slot = idealSlot(entry.hash);
            Entry newEntry = entry;
            newEntry.probeLength = 0;

            while (true) {
                auto& current = entries_[slot];
                if (!current.occupied || current.tombstone) {
                    current = newEntry;
                    ++size_;
                    break;
                }
                if (newEntry.probeLength > current.probeLength) {
                    std::swap(newEntry, current);
                    ++newEntry.probeLength;
                }
                ++newEntry.probeLength;
                slot = (slot + 1) & (entries_.size() - 1);
            }
        }
    }
}

std::vector<uint8_t> HashTable::serialize() const {
    std::vector<uint8_t> out;
    utils::VarInt::encode(static_cast<uint64_t>(entries_.size()), out);
    utils::VarInt::encode(static_cast<uint64_t>(size_), out);

    for (const auto& entry : entries_) {
            out.push_back(entry.occupied ? 1 : 0);
            out.push_back(entry.tombstone ? 1 : 0);
            if (entry.occupied) {
                utils::VarInt::encode(entry.hash, out);
                utils::VarInt::encode(entry.entryId, out);
                utils::VarInt::encode(static_cast<uint64_t>(entry.probeLength), out);
            }
    }

    return out;
}

std::error_code HashTable::deserialize(std::span<const uint8_t> data) {
    size_t offset = 0;

    auto capResult = utils::VarInt::decode(data);
    if (!capResult.valid) return make_error_code(ErrorCode::CorruptIndex);
    size_t capacity = static_cast<size_t>(capResult.value);
    offset += capResult.consumed;

    auto sizeResult = utils::VarInt::decode(data.subspan(offset));
    if (!sizeResult.valid) return make_error_code(ErrorCode::CorruptIndex);
    size_ = static_cast<size_t>(sizeResult.value);
    offset += sizeResult.consumed;

    entries_.resize(capacity);

    for (size_t i = 0; i < capacity; ++i) {
        if (offset >= data.size()) break;
        bool occupied = data[offset++] != 0;
        bool tombstone = data[offset++] != 0;

        if (occupied) {
            auto hashResult = utils::VarInt::decode(data.subspan(offset));
            if (!hashResult.valid) return make_error_code(ErrorCode::CorruptIndex);
            entries_[i].hash = hashResult.value;
            offset += hashResult.consumed;

            auto idResult = utils::VarInt::decode(data.subspan(offset));
            if (!idResult.valid) return make_error_code(ErrorCode::CorruptIndex);
            entries_[i].entryId = static_cast<EntryID>(idResult.value);
            offset += idResult.consumed;

            auto probeResult = utils::VarInt::decode(data.subspan(offset));
            if (!probeResult.valid) return make_error_code(ErrorCode::CorruptIndex);
            entries_[i].probeLength = static_cast<size_t>(probeResult.value);
            offset += probeResult.consumed;

            entries_[i].occupied = true;
            entries_[i].tombstone = tombstone;
        } else {
            entries_[i].occupied = false;
            entries_[i].tombstone = tombstone;
            entries_[i].probeLength = 0;
        }
    }

    return std::error_code();
}

HashTable::Iterator::Iterator(const HashTable* table, size_t index)
    : table_(table), index_(index) {}

std::pair<std::string, EntryID> HashTable::Iterator::operator*() const {
    return {"", table_->entries_[index_].entryId};
}

bool HashTable::Iterator::operator!=(const Iterator& other) const {
    return index_ != other.index_;
}

HashTable::Iterator& HashTable::Iterator::operator++() {
    ++index_;
    while (index_ < table_->entries_.size()) {
        if (table_->entries_[index_].occupied && !table_->entries_[index_].tombstone) {
            break;
        }
        ++index_;
    }
    return *this;
}

HashTable::Iterator HashTable::begin() const {
    size_t first = 0;
    while (first < entries_.size()) {
        if (entries_[first].occupied && !entries_[first].tombstone) break;
        ++first;
    }
    return Iterator(this, first);
}

HashTable::Iterator HashTable::end() const {
    return Iterator(this, entries_.size());
}

} // namespace index
} // namespace nebula
