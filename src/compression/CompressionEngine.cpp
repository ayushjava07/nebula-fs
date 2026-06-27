#include "nebula/compression/CompressionEngine.hpp"

#include <lz4.h>
#include <lz4hc.h>
#include <zlib.h>
#include <zstd.h>

#include <cstring>
#include <algorithm>
#include <system_error>

namespace nebula {
namespace compression {

CompressionEngine::CompressionEngine(CompressionConfig config) : config_(config) {
    initContext();
}

CompressionEngine::~CompressionEngine() noexcept {
    destroyContext();
}

CompressionEngine::CompressionEngine(CompressionEngine&& other) noexcept
    : config_(other.config_), ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}

CompressionEngine& CompressionEngine::operator=(CompressionEngine&& other) noexcept {
    if (this != &other) {
        destroyContext();
        config_ = other.config_;
        ctx_ = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

void CompressionEngine::initContext() {
    switch (config_.algorithm) {
        case CompressionAlgorithm::Zstd:
            ctx_ = ZSTD_createCCtx();
            break;
        case CompressionAlgorithm::Zlib:
        case CompressionAlgorithm::LZ4:
        case CompressionAlgorithm::None:
        default:
            ctx_ = nullptr;
            break;
    }
}

void CompressionEngine::destroyContext() noexcept {
    if (ctx_) {
        switch (config_.algorithm) {
            case CompressionAlgorithm::Zstd:
                ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(ctx_));
                break;
            default:
                break;
        }
        ctx_ = nullptr;
    }
}

CompressionResult CompressionEngine::compress(std::span<const uint8_t> data) {
    switch (config_.algorithm) {
        case CompressionAlgorithm::None: {
            CompressionResult result;
            result.data.assign(data.begin(), data.end());
            result.originalSize = data.size();
            result.compressedSize = data.size();
            result.success = true;
            return result;
        }
        case CompressionAlgorithm::LZ4:
            return compressLZ4(data);
        case CompressionAlgorithm::Zlib:
            return compressZlib(data);
        case CompressionAlgorithm::Zstd:
            return compressZstd(data);
    }
    CompressionResult result;
    result.ec = make_error_code(std::errc::invalid_argument);
    return result;
}

CompressionResult CompressionEngine::decompress(std::span<const uint8_t> compressed,
                                                  size_t originalSize) {
    switch (config_.algorithm) {
        case CompressionAlgorithm::None: {
            CompressionResult result;
            result.data.assign(compressed.begin(), compressed.end());
            result.originalSize = compressed.size();
            result.compressedSize = compressed.size();
            result.success = true;
            return result;
        }
        case CompressionAlgorithm::LZ4:
            return decompressLZ4(compressed, originalSize);
        case CompressionAlgorithm::Zlib:
            return decompressZlib(compressed, originalSize);
        case CompressionAlgorithm::Zstd:
            return decompressZstd(compressed, originalSize);
    }
    CompressionResult result;
    result.ec = make_error_code(std::errc::invalid_argument);
    return result;
}

CompressionResult CompressionEngine::compressLZ4(std::span<const uint8_t> data) {
    CompressionResult result;
    result.originalSize = data.size();

    int maxDestSize = LZ4_compressBound(static_cast<int>(data.size()));
    if (maxDestSize <= 0) {
        result.ec = make_error_code(std::errc::invalid_argument);
        return result;
    }

    result.data.resize(static_cast<size_t>(maxDestSize));
    int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(data.data()),
        reinterpret_cast<char*>(result.data.data()),
        static_cast<int>(data.size()),
        maxDestSize);

    if (compressedSize <= 0) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(static_cast<size_t>(compressedSize));
    result.compressedSize = static_cast<size_t>(compressedSize);
    result.success = true;
    return result;
}

CompressionResult CompressionEngine::decompressLZ4(std::span<const uint8_t> data,
                                                     size_t originalSize) {
    CompressionResult result;
    result.originalSize = originalSize;
    result.compressedSize = data.size();

    result.data.resize(originalSize);
    int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(data.data()),
        reinterpret_cast<char*>(result.data.data()),
        static_cast<int>(data.size()),
        static_cast<int>(originalSize));

    if (decompressedSize < 0) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.success = true;
    return result;
}

CompressionResult CompressionEngine::compressZlib(std::span<const uint8_t> data) {
    CompressionResult result;
    result.originalSize = data.size();

    uLongf destLen = compressBound(static_cast<uLong>(data.size()));
    result.data.resize(destLen);

    int ret = compress2(result.data.data(), &destLen,
                        data.data(), static_cast<uLong>(data.size()),
                        config_.compressionLevel);
    if (ret != Z_OK) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(destLen);
    result.compressedSize = destLen;
    result.success = true;
    return result;
}

CompressionResult CompressionEngine::decompressZlib(std::span<const uint8_t> data,
                                                      size_t originalSize) {
    CompressionResult result;
    result.originalSize = originalSize;
    result.compressedSize = data.size();

    result.data.resize(originalSize);
    uLongf destLen = static_cast<uLongf>(originalSize);

    int ret = uncompress(result.data.data(), &destLen,
                         data.data(), static_cast<uLong>(data.size()));
    if (ret != Z_OK) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.success = true;
    return result;
}

CompressionResult CompressionEngine::compressZstd(std::span<const uint8_t> data) {
    CompressionResult result;
    result.originalSize = data.size();

    size_t maxDestSize = ZSTD_compressBound(data.size());
    result.data.resize(maxDestSize);

    size_t compressedSize = ZSTD_compressCCtx(
        static_cast<ZSTD_CCtx*>(ctx_),
        result.data.data(), maxDestSize,
        data.data(), data.size(),
        config_.compressionLevel);

    if (ZSTD_isError(compressedSize)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(compressedSize);
    result.compressedSize = compressedSize;
    result.success = true;
    return result;
}

CompressionResult CompressionEngine::decompressZstd(std::span<const uint8_t> data,
                                                       size_t originalSize) {
    CompressionResult result;
    result.originalSize = originalSize;
    result.compressedSize = data.size();

    if (originalSize == 0 && !data.empty()) {
        unsigned long long contentSize = ZSTD_getFrameContentSize(data.data(), data.size());
        if (contentSize == ZSTD_CONTENTSIZE_ERROR || contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            result.ec = make_error_code(std::errc::invalid_argument);
            return result;
        }
        originalSize = static_cast<size_t>(contentSize);
        result.originalSize = originalSize;
    }

    result.data.resize(originalSize);
    size_t decompressedSize = ZSTD_decompress(
        result.data.data(), originalSize,
        data.data(), data.size());

    if (ZSTD_isError(decompressedSize)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.success = true;
    return result;
}

std::vector<CompressionResult> CompressionEngine::compressBlocks(
    std::span<const uint8_t> data, size_t blockSize) {
    std::vector<CompressionResult> blocks;
    size_t offset = 0;
    while (offset < data.size()) {
        size_t chunkSize = std::min(blockSize, data.size() - offset);
        auto chunk = data.subspan(offset, chunkSize);
        blocks.push_back(compress(chunk));
        offset += chunkSize;
    }
    return blocks;
}

CompressionResult CompressionEngine::decompressBlocks(
    std::span<const CompressionResult> blocks, size_t totalOriginalSize) {
    CompressionResult result;
    result.originalSize = totalOriginalSize;
    result.success = true;

    std::vector<uint8_t> output;
    output.reserve(totalOriginalSize);

    for (const auto& block : blocks) {
        if (!block.success) {
            result.ec = block.ec;
            result.success = false;
            return result;
        }
        auto decompressed = decompress(block.data, block.originalSize);
        if (!decompressed.success) {
            result.ec = decompressed.ec;
            result.success = false;
            return result;
        }
        output.insert(output.end(), decompressed.data.begin(), decompressed.data.end());
    }

    result.data = std::move(output);
    result.success = true;
    return result;
}

size_t CompressionEngine::estimateCompressedSize(size_t originalSize) const noexcept {
    switch (config_.algorithm) {
        case CompressionAlgorithm::None: return originalSize;
        case CompressionAlgorithm::LZ4:  return static_cast<size_t>(LZ4_compressBound(static_cast<int>(originalSize)));
        case CompressionAlgorithm::Zlib: return compressBound(static_cast<uLong>(originalSize));
        case CompressionAlgorithm::Zstd: return ZSTD_compressBound(originalSize);
    }
    return originalSize;
}

bool CompressionEngine::isSupported() const noexcept {
    return true;
}

size_t CompressionEngine::maxCompressedSize(size_t inputSize) noexcept {
    return ZSTD_compressBound(inputSize);
}

void CompressionEngine::setConfig(const CompressionConfig& config) {
    destroyContext();
    config_ = config;
    initContext();
}

std::string_view CompressionEngine::algorithmName() const noexcept {
    switch (config_.algorithm) {
        case CompressionAlgorithm::None: return "none";
        case CompressionAlgorithm::LZ4:  return "LZ4";
        case CompressionAlgorithm::Zlib: return "Zlib";
        case CompressionAlgorithm::Zstd: return "Zstd";
    }
    return "unknown";
}

} // namespace compression
} // namespace nebula
