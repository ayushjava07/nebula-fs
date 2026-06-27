#include <cstdint>
#include <cstddef>
#include <vector>
#include "nebula/parser/MetadataParser.hpp"
#include "nebula/metadata/MetadataStore.hpp"
#include "nebula/metadata/EntryMetadata.hpp"

/// Fuzz harness for metadata parsing.
///
/// Tests MetadataParser, MetadataStore, and EntryMetadata
/// deserialization with arbitrary byte sequences.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Fuzz MetadataParser
    {
        nebula::parser::MetadataParser metaParser;
        auto result = metaParser.parse(
            std::span<const uint8_t>(data, size), size);
        (void)result;

        // Try validation
        auto ec = metaParser.validate(
            std::span<const uint8_t>(data, size), size);
        (void)ec;

        nebula::parser::MetadataParser::quickValidate(
            std::span<const uint8_t>(data, size));
    }

    // Fuzz MetadataStore deserialization
    {
        nebula::metadata::MetadataStore store;
        auto ec = store.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;

        if (!ec) {
            store.validate();
            volatile auto count = store.size();
            (void)count;
        }
    }

    // Fuzz EntryMetadata deserialization
    {
        nebula::metadata::EntryMetadata entryMeta;
        auto ec = entryMeta.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;
    }

    // Test serialization round-trip on a small valid store
    if (size < 256) {
        nebula::metadata::MetadataStore store;
        store.set("key", std::span<const uint8_t>(data, size));
        auto serialized = store.serialize();

        nebula::metadata::MetadataStore parsed;
        parsed.deserialize(serialized);
    }

    return 0;
}
