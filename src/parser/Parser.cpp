#include "nebula/parser/Parser.hpp"
#include "nebula/parser/MetadataParser.hpp"
#include "nebula/utils/Checksum.hpp"
#include "nebula/utils/MemoryMappedFile.hpp"

#include <cstring>
#include <system_error>
#include <fstream>

namespace nebula {
namespace parser {

Parser::Parser(ParserConfig config) : config_(config) {}

Parser::~Parser() noexcept = default;

Parser::Parser(Parser&& other) noexcept
    : config_(other.config_)
    , state_(other.state_)
    , warnings_(std::move(other.warnings_)) {}

Parser& Parser::operator=(Parser&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
        state_ = other.state_;
        warnings_ = std::move(other.warnings_);
    }
    return *this;
}

Result<ParseResult> Parser::parse(std::span<const uint8_t> data) {
    reset();
    return parseInternal(data);
}

Result<ParseResult> Parser::parseFile(const std::string& path) {
    reset();

    utils::MemoryMappedFile mappedFile(path);
    if (!mappedFile.isOpen()) {
        return toParseError(ErrorCode::IOError, ParserState::Init, 0,
                           "cannot open file: " + path);
    }

    return parseInternal(mappedFile.span());
}

Result<archive::ArchiveHeader> Parser::parseHeader(std::span<const uint8_t> data) {
    archive::ArchiveHeader header;
    auto ec = header.parse(data);
    if (ec) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0, ec.message());
    }
    return header;
}

Result<metadata::MetadataStore> Parser::parseMetadata(std::span<const uint8_t> data,
                                                        const archive::ArchiveHeader& header) {
    if (!header.isValid()) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0);
    }

    if (header.metadataOffset() + header.metadataSize() > data.size()) {
        return toParseError(ErrorCode::OutOfRange, ParserState::Metadata,
                           header.metadataOffset());
    }

    auto metadataSpan = data.subspan(
        static_cast<size_t>(header.metadataOffset()),
        static_cast<size_t>(header.metadataSize()));

    MetadataParser metaParser;
    return metaParser.parse(metadataSpan, static_cast<size_t>(header.metadataSize()));
}

Result<filesystem::DirectoryTree> Parser::parseDirectoryTree(std::span<const uint8_t> data,
                                                               const archive::ArchiveHeader& header) {
    if (!header.isValid()) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0);
    }

    if (header.directoryOffset() + header.directorySize() > data.size()) {
        return toParseError(ErrorCode::OutOfRange, ParserState::DirectoryTree,
                           header.directoryOffset());
    }

    auto dirSpan = data.subspan(
        static_cast<size_t>(header.directoryOffset()),
        static_cast<size_t>(header.directorySize()));

    filesystem::DirectoryTree tree;
    auto ec = tree.deserialize(dirSpan);
    if (ec) {
        return toParseError(ErrorCode::CorruptDirectory, ParserState::DirectoryTree,
                           header.directoryOffset(), ec.message());
    }

    return tree;
}

Result<index::IndexManager> Parser::parseIndexTable(std::span<const uint8_t> data,
                                                      const archive::ArchiveHeader& header) {
    if (!header.isValid()) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0);
    }

    if (header.indexOffset() + header.indexSize() > data.size()) {
        return toParseError(ErrorCode::OutOfRange, ParserState::IndexTable,
                           header.indexOffset());
    }

    auto indexSpan = data.subspan(
        static_cast<size_t>(header.indexOffset()),
        static_cast<size_t>(header.indexSize()));

    index::IndexManager indexMgr;
    auto ec = indexMgr.deserialize(indexSpan);
    if (ec) {
        return toParseError(ErrorCode::CorruptIndex, ParserState::IndexTable,
                           header.indexOffset(), ec.message());
    }

    return indexMgr;
}

Result<storage::ChunkManager> Parser::parseChunkTable(std::span<const uint8_t> data,
                                                       const archive::ArchiveHeader& header) {
    if (!header.isValid()) {
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header, 0);
    }

    if (header.chunkOffset() + header.chunkSize() > data.size()) {
        return toParseError(ErrorCode::OutOfRange, ParserState::ChunkTable,
                           header.chunkOffset());
    }

    auto chunkSpan = data.subspan(
        static_cast<size_t>(header.chunkOffset()),
        static_cast<size_t>(header.chunkSize()));

    storage::ChunkManager chunkMgr;
    auto ec = chunkMgr.deserialize(chunkSpan);
    if (ec) {
        return toParseError(ErrorCode::CorruptChunkTable, ParserState::ChunkTable,
                           header.chunkOffset(), ec.message());
    }

    return chunkMgr;
}

void Parser::reset() {
    state_ = ParserState::Init;
    warnings_.clear();
}

void Parser::addWarning(ParseError err) {
    if (config_.strictMode && err.severity == ErrorSeverity::Warning) {
        err.severity = ErrorSeverity::Recoverable;
    }
    warnings_.push_back(std::move(err));
}

Result<ParseResult> Parser::parseInternal(std::span<const uint8_t> data) {
    ParseResult result;

    if (data.size() < sizeof(format::ArchiveHeader)) {
        state_ = ParserState::Error;
        return toParseError(ErrorCode::CorruptHeader, ParserState::Header,
                           static_cast<uint64_t>(data.size()));
    }

    state_ = ParserState::Header;
    auto headerResult = parseHeader(data);
    if (isError(headerResult)) {
        state_ = ParserState::Error;
        return getError(headerResult);
    }
    result.header = getValue(headerResult);
    state_ = ParserState::Metadata;

    if (result.header.metadataSize() > 0) {
        auto metaResult = parseMetadata(data, result.header);
        if (isError(metaResult)) {
            addWarning(getError(metaResult));
            if (config_.strictMode) {
                state_ = ParserState::Error;
                return getError(metaResult);
            }
        } else {
            result.metadata = getValue(metaResult);
        }
    }
    state_ = ParserState::DirectoryTree;

    if (result.header.directorySize() > 0) {
        auto dirResult = parseDirectoryTree(data, result.header);
        if (isError(dirResult)) {
            addWarning(getError(dirResult));
            if (config_.strictMode) {
                state_ = ParserState::Error;
                return getError(dirResult);
            }
        } else {
            result.directoryTree = getValue(std::move(dirResult));
        }
    }
    state_ = ParserState::IndexTable;

    if (result.header.indexSize() > 0) {
        auto indexResult = parseIndexTable(data, result.header);
        if (isError(indexResult)) {
            addWarning(getError(indexResult));
            if (config_.strictMode) {
                state_ = ParserState::Error;
                return getError(indexResult);
            }
        } else {
            result.indexManager = getValue(std::move(indexResult));
        }
    }
    state_ = ParserState::ChunkTable;

    if (result.header.chunkSize() > 0) {
        auto chunkResult = parseChunkTable(data, result.header);
        if (isError(chunkResult)) {
            addWarning(getError(chunkResult));
            if (config_.strictMode) {
                state_ = ParserState::Error;
                return getError(chunkResult);
            }
        } else {
            result.chunkManager = getValue(std::move(chunkResult));
        }
    }
    state_ = ParserState::CompressedBlocks;

    if (result.header.blocksOffset() + result.header.blocksSize() <= data.size()) {
        result.blocksData.assign(
            data.begin() + static_cast<ptrdiff_t>(result.header.blocksOffset()),
            data.begin() + static_cast<ptrdiff_t>(result.header.blocksOffset() + result.header.blocksSize()));
    }
    state_ = ParserState::ObjectRecon;

    result.valid = true;
    result.warnings = warnings_;
    state_ = ParserState::Complete;

    return result;
}

bool Parser::validateSectionBounds(uint64_t offset, uint64_t size,
                                    uint64_t archiveSize, uint64_t offsetForError) const {
    if (offset + size > archiveSize) {
        return false;
    }
    if (offset < sizeof(format::ArchiveHeader)) {
        return false;
    }
    return true;
}

} // namespace parser
} // namespace nebula
