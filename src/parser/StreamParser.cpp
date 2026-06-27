#include "nebula/parser/StreamParser.hpp"
#include "nebula/parser/MetadataParser.hpp"
#include "nebula/archive/ArchiveHeader.hpp"
#include "nebula/utils/Checksum.hpp"

#include <cstring>
#include <system_error>

namespace nebula {
namespace parser {

StreamParser::StreamParser(ParserConfig config) : config_(config) {
    buffer_.reserve(kDefaultBufferSize);
}

StreamParser::~StreamParser() noexcept = default;

StreamParser::StreamParser(StreamParser&& other) noexcept
    : config_(other.config_)
    , state_(other.state_)
    , result_(std::move(other.result_))
    , buffer_(std::move(other.buffer_))
    , warnings_(std::move(other.warnings_))
    , bytesProcessed_(other.bytesProcessed_)
    , expectedSize_(other.expectedSize_) {}

StreamParser& StreamParser::operator=(StreamParser&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
        state_ = other.state_;
        result_ = std::move(other.result_);
        buffer_ = std::move(other.buffer_);
        warnings_ = std::move(other.warnings_);
        bytesProcessed_ = other.bytesProcessed_;
        expectedSize_ = other.expectedSize_;
    }
    return *this;
}

Result<size_t> StreamParser::feed(std::span<const uint8_t> chunk) {
    buffer_.insert(buffer_.end(), chunk.begin(), chunk.end());
    size_t totalConsumed = 0;

    while (state_ != ParserState::Complete && state_ != ParserState::Error) {
        auto prevState = state_;
        Result<size_t> consumed{static_cast<size_t>(0)};

        switch (state_) {
            case ParserState::Init:
            case ParserState::Header:
                consumed = processHeader();
                break;
            case ParserState::Metadata:
                consumed = processMetadata();
                break;
            case ParserState::DirectoryTree:
                consumed = processDirectoryTree();
                break;
            case ParserState::IndexTable:
                consumed = processIndexTable();
                break;
            case ParserState::ChunkTable:
                consumed = processChunkTable();
                break;
            case ParserState::CompressedBlocks:
                consumed = processCompressedBlocks();
                break;
            case ParserState::ObjectRecon:
                consumed = finalize();
                break;
            default:
                return toParseError(ErrorCode::InternalError, state_,
                                   bytesProcessed_);
        }

        if (isError(consumed)) {
            state_ = ParserState::Error;
            return consumed;
        }

        size_t consumedBytes = getValue(consumed);
        totalConsumed += consumedBytes;
        if (consumedBytes == 0 && prevState == state_) break;
    }

    return totalConsumed;
}

void StreamParser::reset() {
    state_ = ParserState::Init;
    result_ = ParseResult{};
    buffer_.clear();
    warnings_.clear();
    bytesProcessed_ = 0;
    expectedSize_ = 0;
}

double StreamParser::progress() const noexcept {
    if (expectedSize_ == 0) return 0.0;
    return static_cast<double>(bytesProcessed_) / static_cast<double>(expectedSize_);
}

bool StreamParser::haveBytes(size_t needed) const noexcept {
    return buffer_.size() >= needed;
}

void StreamParser::transitionTo(ParserState newState) {
    state_ = newState;
}

Result<size_t> StreamParser::processHeader() {
    if (!haveBytes(sizeof(format::ArchiveHeader))) {
        return static_cast<size_t>(0);
    }

    archive::ArchiveHeader header;
    auto ec = header.parse(buffer_);
    if (ec) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0, ec.message());
    }

    result_.header = std::move(header);
    expectedSize_ = static_cast<size_t>(result_.header.archiveSize());
    buffer_.erase(buffer_.begin(), buffer_.begin() + sizeof(format::ArchiveHeader));
    bytesProcessed_ += sizeof(format::ArchiveHeader);
    transitionTo(ParserState::Metadata);
    return sizeof(format::ArchiveHeader);
}

Result<size_t> StreamParser::processMetadata() {
    if (result_.header.metadataSize() == 0) {
        transitionTo(ParserState::DirectoryTree);
        return static_cast<size_t>(0);
    }

    auto metaSize = static_cast<size_t>(result_.header.metadataSize());
    if (!haveBytes(metaSize)) {
        return static_cast<size_t>(0);
    }

    MetadataParser metaParser;
    auto metaResult = metaParser.parse(buffer_, metaSize);
    if (!isError(metaResult)) {
        result_.metadata = getValue(metaResult);
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(metaSize));
    bytesProcessed_ += metaSize;
    transitionTo(ParserState::DirectoryTree);
    return metaSize;
}

Result<size_t> StreamParser::processDirectoryTree() {
    if (result_.header.directorySize() == 0) {
        transitionTo(ParserState::IndexTable);
        return static_cast<size_t>(0);
    }

    auto dirSize = static_cast<size_t>(result_.header.directorySize());
    if (!haveBytes(dirSize)) {
        return static_cast<size_t>(0);
    }

    auto ec = result_.directoryTree.deserialize(
        std::span<const uint8_t>(buffer_.data(), dirSize));
    if (ec) {
        warnings_.push_back(toParseError(ErrorCode::CorruptDirectory, ParserState::DirectoryTree,
                                         bytesProcessed_, ec.message()));
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(dirSize));
    bytesProcessed_ += dirSize;
    transitionTo(ParserState::IndexTable);
    return dirSize;
}

Result<size_t> StreamParser::processIndexTable() {
    if (result_.header.indexSize() == 0) {
        transitionTo(ParserState::ChunkTable);
        return static_cast<size_t>(0);
    }

    auto idxSize = static_cast<size_t>(result_.header.indexSize());
    if (!haveBytes(idxSize)) {
        return static_cast<size_t>(0);
    }

    auto ec = result_.indexManager.deserialize(
        std::span<const uint8_t>(buffer_.data(), idxSize));
    if (ec) {
        warnings_.push_back(toParseError(ErrorCode::CorruptIndex, ParserState::IndexTable,
                                         bytesProcessed_, ec.message()));
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(idxSize));
    bytesProcessed_ += idxSize;
    transitionTo(ParserState::ChunkTable);
    return idxSize;
}

Result<size_t> StreamParser::processChunkTable() {
    if (result_.header.chunkSize() == 0) {
        transitionTo(ParserState::CompressedBlocks);
        return static_cast<size_t>(0);
    }

    auto chunkSize = static_cast<size_t>(result_.header.chunkSize());
    if (!haveBytes(chunkSize)) {
        return static_cast<size_t>(0);
    }

    auto ec = result_.chunkManager.deserialize(
        std::span<const uint8_t>(buffer_.data(), chunkSize));
    if (ec) {
        warnings_.push_back(toParseError(ErrorCode::CorruptChunkTable, ParserState::ChunkTable,
                                         bytesProcessed_, ec.message()));
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(chunkSize));
    bytesProcessed_ += chunkSize;
    transitionTo(ParserState::CompressedBlocks);
    return chunkSize;
}

Result<size_t> StreamParser::processCompressedBlocks() {
    if (result_.header.blocksSize() == 0) {
        transitionTo(ParserState::ObjectRecon);
        return static_cast<size_t>(0);
    }

    auto blocksSize = static_cast<size_t>(result_.header.blocksSize());
    if (!haveBytes(blocksSize)) {
        return static_cast<size_t>(0);
    }

    result_.blocksData.assign(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(blocksSize));
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(blocksSize));
    bytesProcessed_ += blocksSize;
    transitionTo(ParserState::ObjectRecon);
    return blocksSize;
}

Result<size_t> StreamParser::finalize() {
    result_.valid = true;
    result_.warnings = warnings_;
    transitionTo(ParserState::Complete);
    return static_cast<size_t>(0);
}

} // namespace parser
} // namespace nebula
