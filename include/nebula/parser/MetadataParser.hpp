#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "../metadata/MetadataStore.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <span>
#include <system_error>

namespace nebula {
namespace parser {

/// Specialized parser for the metadata section of a NebulaFS archive.
///
/// Handles key-value metadata parsing with support for:
/// - String and binary values
/// - Varint-encoded length prefixes
/// - Graceful recovery from truncated or corrupt sections
class MetadataParser {
public:
    MetadataParser() = default;

    [[nodiscard]] Result<metadata::MetadataStore> parse(
        std::span<const uint8_t> data, size_t metadataSize);

    [[nodiscard]] Result<std::pair<std::string, std::vector<uint8_t>>> parseEntry(
        std::span<const uint8_t> data, size_t& offset);

    [[nodiscard]] std::error_code validate(
        std::span<const uint8_t> data, size_t metadataSize);

    [[nodiscard]] std::vector<uint8_t> serialize(const metadata::MetadataStore& metadata);

    [[nodiscard]] static bool quickValidate(std::span<const uint8_t> data) noexcept;

    [[nodiscard]] const ParseError& lastError() const noexcept { return lastError_; }

private:
    ParseError lastError_;
};

} // namespace parser
} // namespace nebula
