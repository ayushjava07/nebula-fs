#include "nebula/parser/MetadataParser.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>

namespace nebula {
namespace parser {

Result<metadata::MetadataStore> MetadataParser::parse(std::span<const uint8_t> data,
                                             size_t metadataSize) {
    if (data.size() < metadataSize) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(data.size()),
                           "metadata section truncated");
    }

    metadata::MetadataStore store;
    size_t offset = 0;

    while (offset < metadataSize) {
        auto entryResult = parseEntry(data, offset);
        if (isError(entryResult)) {
            if (offset == 0) {
                return getError(entryResult);
            }
            break;
        }

        auto& entry = getValue(entryResult);
        store.set(entry.first, entry.second);
    }

    return store;
}

Result<std::pair<std::string, std::vector<uint8_t>>> MetadataParser::parseEntry(
    std::span<const uint8_t> data, size_t& offset) {
    std::pair<std::string, std::vector<uint8_t>> result;
    size_t startOffset = offset;

    auto span = data.subspan(offset);

    auto keyLenResult = utils::VarInt::decode(span);
    if (!keyLenResult.valid || keyLenResult.consumed == 0) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(offset));
    }

    if (keyLenResult.value > kMaxFileNameLength) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(offset), "metadata key too long");
    }

    size_t pos = keyLenResult.consumed;
    auto valLenResult = utils::VarInt::decode(span.subspan(pos));
    if (!valLenResult.valid) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(offset));
    }

    if (valLenResult.value > kMaxMetadataSize) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(offset), "metadata value too large");
    }

    pos += valLenResult.consumed;

    size_t keyLen = static_cast<size_t>(keyLenResult.value);
    if (startOffset + pos + keyLen > data.size()) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(pos));
    }

    result.first.assign(reinterpret_cast<const char*>(&span[pos]), keyLen);
    pos += keyLen;

    size_t valLen = static_cast<size_t>(valLenResult.value);
    if (startOffset + pos + valLen > data.size()) {
        return toParseError(ErrorCode::CorruptMetadata, ParserState::Metadata,
                           static_cast<uint64_t>(pos));
    }

    result.second.assign(&span[pos], &span[pos + valLen]);
    pos += valLen;

    offset = startOffset + pos;

    lastError_ = ParseError{};
    return result;
}

std::error_code MetadataParser::validate(std::span<const uint8_t> data, size_t metadataSize) {
    if (data.size() < metadataSize) {
        return make_error_code(ErrorCode::CorruptMetadata);
    }

    auto result = parse(data, metadataSize);
    if (isError(result)) {
        return make_error_code(ErrorCode::CorruptMetadata);
    }

    return std::error_code();
}

std::vector<uint8_t> MetadataParser::serialize(const metadata::MetadataStore& metadata) {
    std::vector<uint8_t> result;
    const auto& entries = metadata.entries();

    for (const auto& [key, value] : entries) {
        utils::VarInt::encode(static_cast<uint64_t>(key.size()), result);
        utils::VarInt::encode(static_cast<uint64_t>(value.size()), result);
        result.insert(result.end(), key.begin(), key.end());
        result.insert(result.end(), value.begin(), value.end());
    }

    return result;
}

bool MetadataParser::quickValidate(std::span<const uint8_t> data) noexcept {
    if (data.empty()) return true;

    constexpr size_t kMinEntryOverhead = 2;
    if (data.size() < kMinEntryOverhead) return false;

    return true;
}

} // namespace parser
} // namespace nebula
