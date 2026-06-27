#include <cstdint>
#include <cstddef>
#include <vector>
#include "nebula/parser/Parser.hpp"

/// Fuzz harness for the archive parser.
///
/// Exercises the full parsing pipeline with arbitrary byte sequences.
/// The parser must handle malformed input gracefully without crashing.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    nebula::parser::ParserConfig config;
    config.verifyChecksums = true;
    config.lazyMode = true;
    config.strictMode = false;

    nebula::parser::Parser parser(config);
    auto result = parser.parse(std::span<const uint8_t>(data, size));

    // Exercise section parsers individually if the header is valid
    if (!nebula::isError(result)) {
        auto& parseResult = nebula::getValue(result);

        // Access parsed data to ensure no lazy evaluation issues
        volatile auto entryCount = parseResult.header.entryCount();
        (void)entryCount;

        // Try extracting metadata
        if (parseResult.header.metadataSize() > 0 &&
            parseResult.header.metadataSize() < size) {
            auto metaResult = parser.parseMetadata(
                std::span<const uint8_t>(data, size), parseResult.header);
            (void)metaResult;
        }
    }

    return 0;
}
