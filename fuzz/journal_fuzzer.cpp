#include <cstdint>
#include <cstddef>
#include <vector>
#include "nebula/filesystem/JournalManager.hpp"
#include "nebula/filesystem/Recovery.hpp"

/// Fuzz harness for the journal and recovery subsystem.
///
/// Tests deserialization of journal data and recovery operations
/// with arbitrary byte sequences to ensure crash safety.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Fuzz JournalManager deserialization
    {
        nebula::filesystem::JournalManager jm;
        auto ec = jm.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;

        if (!ec) {
            // Test journal operations on deserialized data
            jm.verifyIntegrity();

            // Attempt recovery
            nebula::filesystem::Recovery recovery;
            auto analysis = recovery.analyze(jm);
            (void)analysis;
        }
    }

    // Fuzz with partial, corrupted data
    if (size > 0) {
        nebula::filesystem::JournalManager jm;
        auto ec = jm.deserialize(std::span<const uint8_t>(data, size / 2));
        (void)ec;
    }

    return 0;
}
