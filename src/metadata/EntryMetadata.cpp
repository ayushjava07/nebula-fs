#include "nebula/metadata/EntryMetadata.hpp"
#include "nebula/utils/VarInt.hpp"
#include <algorithm>
#include <cstring>
#include <system_error>

namespace nebula {
namespace metadata {

void EntryMetadata::setAttribute(const std::string& name, const std::string& value) {
    attributes_[name] = std::vector<uint8_t>(value.begin(), value.end());
}

void EntryMetadata::setAttribute(const std::string& name, std::span<const uint8_t> value) {
    attributes_[name] = std::vector<uint8_t>(value.begin(), value.end());
}

std::optional<std::string> EntryMetadata::getAttribute(const std::string& name) const {
    auto it = attributes_.find(name);
    if (it == attributes_.end()) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(it->second.data()), it->second.size());
}

std::vector<std::string> EntryMetadata::attributeNames() const {
    std::vector<std::string> names;
    names.reserve(attributes_.size());
    for (const auto& [name, _] : attributes_) {
        names.push_back(name);
    }
    return names;
}

bool EntryMetadata::hasAttribute(const std::string& name) const noexcept {
    return attributes_.find(name) != attributes_.end();
}

bool EntryMetadata::removeAttribute(const std::string& name) noexcept {
    return attributes_.erase(name) > 0;
}

void EntryMetadata::setTags(const std::vector<std::string>& tags) {
    tags_ = tags;
}

void EntryMetadata::addTag(const std::string& tag) {
    if (!hasTag(tag)) {
        tags_.push_back(tag);
    }
}

bool EntryMetadata::removeTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it == tags_.end()) return false;
    tags_.erase(it);
    return true;
}

bool EntryMetadata::hasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

void EntryMetadata::clear() noexcept {
    attributes_.clear();
    tags_.clear();
    contentType_.clear();
    sourcePath_.clear();
    notes_.clear();
}

std::vector<uint8_t> EntryMetadata::serialize() const {
    std::vector<uint8_t> result;

    utils::VarInt::encode(static_cast<uint64_t>(attributes_.size()), result);
    for (const auto& [name, value] : attributes_) {
        utils::VarInt::encode(static_cast<uint64_t>(name.size()), result);
        utils::VarInt::encode(static_cast<uint64_t>(value.size()), result);
        result.insert(result.end(), name.begin(), name.end());
        result.insert(result.end(), value.begin(), value.end());
    }

    utils::VarInt::encode(static_cast<uint64_t>(tags_.size()), result);
    for (const auto& tag : tags_) {
        utils::VarInt::encode(static_cast<uint64_t>(tag.size()), result);
        result.insert(result.end(), tag.begin(), tag.end());
    }

    utils::VarInt::encode(static_cast<uint64_t>(contentType_.size()), result);
    result.insert(result.end(), contentType_.begin(), contentType_.end());

    utils::VarInt::encode(static_cast<uint64_t>(sourcePath_.size()), result);
    result.insert(result.end(), sourcePath_.begin(), sourcePath_.end());

    utils::VarInt::encode(static_cast<uint64_t>(notes_.size()), result);
    result.insert(result.end(), notes_.begin(), notes_.end());

    return result;
}

std::error_code EntryMetadata::deserialize(std::span<const uint8_t> data) {
    clear();

    constexpr size_t kMaxStringSize = 1024 * 1024;

    size_t offset = 0;
    if (offset >= data.size()) return std::error_code();

    auto attrCountResult = utils::VarInt::decode(data.subspan(offset));
    if (!attrCountResult.valid) return make_error_code(ErrorCode::CorruptMetadata);
    offset += attrCountResult.consumed;

    size_t attrCount = static_cast<size_t>(attrCountResult.value);
    for (size_t i = 0; i < attrCount; ++i) {
        auto nameLenResult = utils::VarInt::decode(data.subspan(offset));
        if (!nameLenResult.valid) return make_error_code(ErrorCode::CorruptMetadata);
        offset += nameLenResult.consumed;

        auto valLenResult = utils::VarInt::decode(data.subspan(offset));
        if (!valLenResult.valid) return make_error_code(ErrorCode::CorruptMetadata);
        offset += valLenResult.consumed;

        size_t nameLen = static_cast<size_t>(nameLenResult.value);
        size_t valLen = static_cast<size_t>(valLenResult.value);

        if (offset > data.size()) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }
        size_t remaining = data.size() - offset;
        if (nameLen > remaining || valLen > remaining - nameLen) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }
        if (nameLen > kMaxStringSize || valLen > kMaxStringSize) {
            return make_error_code(ErrorCode::CorruptMetadata);
        }

        std::string name(reinterpret_cast<const char*>(&data[offset]), nameLen);
        offset += nameLen;

        std::vector<uint8_t> value(data.begin() + static_cast<ptrdiff_t>(offset),
                                   data.begin() + static_cast<ptrdiff_t>(offset + valLen));
        offset += valLen;

        attributes_[name] = std::move(value);
    }

    if (offset >= data.size()) return std::error_code();
    auto tagCountResult = utils::VarInt::decode(data.subspan(offset));
    if (tagCountResult.valid) {
        offset += tagCountResult.consumed;
        size_t tagCount = static_cast<size_t>(tagCountResult.value);
        for (size_t i = 0; i < tagCount; ++i) {
            auto tagLenResult = utils::VarInt::decode(data.subspan(offset));
            if (!tagLenResult.valid) break;
            offset += tagLenResult.consumed;
            size_t tagLen = static_cast<size_t>(tagLenResult.value);
            if (offset > data.size()) break;
            if (tagLen > data.size() - offset) break;
            if (tagLen > kMaxStringSize) break;
            tags_.emplace_back(reinterpret_cast<const char*>(&data[offset]), tagLen);
            offset += tagLen;
        }
    }

    if (offset >= data.size()) return std::error_code();
    auto ctLenResult = utils::VarInt::decode(data.subspan(offset));
    if (ctLenResult.valid) {
        offset += ctLenResult.consumed;
        size_t ctLen = static_cast<size_t>(ctLenResult.value);
        if (offset <= data.size() && ctLen <= data.size() - offset && ctLen <= kMaxStringSize) {
            contentType_ = std::string(reinterpret_cast<const char*>(&data[offset]), ctLen);
            offset += ctLen;
        }
    }

    if (offset >= data.size()) return std::error_code();
    auto spLenResult = utils::VarInt::decode(data.subspan(offset));
    if (spLenResult.valid) {
        offset += spLenResult.consumed;
        size_t spLen = static_cast<size_t>(spLenResult.value);
        if (offset <= data.size() && spLen <= data.size() - offset && spLen <= kMaxStringSize) {
            sourcePath_ = std::string(reinterpret_cast<const char*>(&data[offset]), spLen);
            offset += spLen;
        }
    }

    if (offset >= data.size()) return std::error_code();
    auto nLenResult = utils::VarInt::decode(data.subspan(offset));
    if (nLenResult.valid) {
        offset += nLenResult.consumed;
        size_t nLen = static_cast<size_t>(nLenResult.value);
        if (offset <= data.size() && nLen <= data.size() - offset && nLen <= kMaxStringSize) {
            notes_ = std::string(reinterpret_cast<const char*>(&data[offset]), nLen);
            offset += nLen;
        }
    }

    return std::error_code();
}

} // namespace metadata
} // namespace nebula
