#include "nebula/metadata/MetadataStore.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>

namespace nebula {
namespace metadata {

void MetadataStore::set(const std::string& key, const std::string& value) {
    entries_[key] = std::vector<uint8_t>(value.begin(), value.end());
}

void MetadataStore::set(const std::string& key, std::span<const uint8_t> value) {
    entries_[key] = std::vector<uint8_t>(value.begin(), value.end());
}

std::optional<std::string> MetadataStore::getString(const std::string& key) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(it->second.data()), it->second.size());
}

std::optional<std::vector<uint8_t>> MetadataStore::getBinary(const std::string& key) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

bool MetadataStore::contains(const std::string& key) const noexcept {
    return entries_.find(key) != entries_.end();
}

bool MetadataStore::remove(const std::string& key) noexcept {
    return entries_.erase(key) > 0;
}

std::vector<std::string> MetadataStore::keys() const {
    std::vector<std::string> result;
    result.reserve(entries_.size());
    for (const auto& [key, _] : entries_) {
        result.push_back(key);
    }
    return result;
}

void MetadataStore::clear() noexcept {
    entries_.clear();
}

std::vector<uint8_t> MetadataStore::serialize() const {
    std::vector<uint8_t> result;

    for (const auto& [key, value] : entries_) {
        utils::VarInt::encode(static_cast<uint64_t>(key.size()), result);
        utils::VarInt::encode(static_cast<uint64_t>(value.size()), result);
        result.insert(result.end(), key.begin(), key.end());
        result.insert(result.end(), value.begin(), value.end());
    }

    return result;
}

std::error_code MetadataStore::deserialize(std::span<const uint8_t> data) {
    clear();

    size_t offset = 0;
    while (offset < data.size()) {
        auto keyLenResult = utils::VarInt::decode(data.subspan(offset));
        if (!keyLenResult.valid) break;
        offset += keyLenResult.consumed;

        auto valLenResult = utils::VarInt::decode(data.subspan(offset));
        if (!valLenResult.valid) break;
        offset += valLenResult.consumed;

        size_t keyLen = static_cast<size_t>(keyLenResult.value);
        size_t valLen = static_cast<size_t>(valLenResult.value);

        constexpr size_t kMaxEntrySize = 1024 * 1024;

        if (offset > data.size()) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }
        size_t remaining = data.size() - offset;
        if (keyLen > remaining || valLen > remaining - keyLen) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }
        if (keyLen > kMaxEntrySize || valLen > kMaxEntrySize) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }

        std::string key(reinterpret_cast<const char*>(&data[offset]), keyLen);
        offset += keyLen;

        std::vector<uint8_t> value(data.begin() + static_cast<ptrdiff_t>(offset),
                                   data.begin() + static_cast<ptrdiff_t>(offset + valLen));
        offset += valLen;

        entries_[key] = std::move(value);
    }

    return std::error_code();
}

bool MetadataStore::validate() const noexcept {
    for (const auto& [key, value] : entries_) {
        if (key.empty()) return false;
        if (value.size() > kMaxMetadataSize) return false;
    }
    return true;
}

void MetadataStore::merge(const MetadataStore& other) {
    for (const auto& [key, value] : other.entries_) {
        if (entries_.find(key) == entries_.end()) {
            entries_[key] = value;
        }
    }
}

} // namespace metadata
} // namespace nebula
