#include <cstdint>
#include <cstddef>
#include <vector>
#include "nebula/compression/CompressionEngine.hpp"

/// Fuzz harness for the compression engine.
///
/// Tests all compression algorithms with arbitrary byte sequences.
/// Verifies that compress/decompress operations are memory-safe
/// even with pathological inputs.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip very large inputs to avoid timeout
    if (size > 65536) return 0;

    std::span<const uint8_t> input(data, size);

    // Test each compression algorithm
    nebula::CompressionAlgorithm algos[] = {
        nebula::CompressionAlgorithm::LZ4,
        nebula::CompressionAlgorithm::Zlib,
        nebula::CompressionAlgorithm::Zstd
    };

    for (auto algo : algos) {
        nebula::compression::CompressionConfig config;
        config.algorithm = algo;
        config.compressionLevel = 1;

        nebula::compression::CompressionEngine engine(config);

        // Compress the fuzz input
        auto compResult = engine.compress(input);
        if (compResult.success && !input.empty()) {
            // Try decompressing the compressed output
            auto decompResult = engine.decompress(
                compResult.data, input.size());
            (void)decompResult;
        }

        // Test block compression
        if (!input.empty()) {
            auto blocks = engine.compressBlocks(input, 64);
            if (!blocks.empty()) {
                auto decompBlocks = engine.decompressBlocks(
                    blocks, input.size());
                (void)decompBlocks;
            }
        }
    }

    return 0;
}
