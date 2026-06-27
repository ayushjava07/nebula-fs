#include <cstdint>
#include <cstddef>
#include <vector>
#include "nebula/index/IndexManager.hpp"
#include "nebula/index/BTree.hpp"
#include "nebula/index/HashTable.hpp"

/// Fuzz harness for the index subsystem.
///
/// Tests deserialization of IndexManager, BTree, and HashTable
/// with arbitrary byte sequences to ensure memory safety.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Fuzz IndexManager deserialization
    {
        nebula::index::IndexManager mgr;
        auto ec = mgr.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;

        // If deserialization succeeded, test basic operations
        if (!ec) {
            volatile auto count = mgr.size();
            (void)count;
            mgr.validate();
        }
    }

    // Fuzz HashTable deserialization
    {
        nebula::index::HashTable ht;
        auto ec = ht.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;
    }

    // Fuzz BTree deserialization
    if (size > 0) {
        nebula::index::BTree<uint64_t, nebula::IndexEntry> tree;
        auto ec = tree.deserialize(std::span<const uint8_t>(data, size));
        (void)ec;

        if (!ec) {
            volatile auto count = tree.size();
            (void)count;
        }
    }

    return 0;
}
