#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "../archive/ArchiveHeader.hpp"
#include "../metadata/MetadataStore.hpp"
#include "../filesystem/DirectoryTree.hpp"
#include "../index/IndexManager.hpp"
#include "../storage/ChunkManager.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>
#include <functional>

namespace nebula {
namespace parser {

/// Configuration for the archive parser.
struct ParserConfig {
    bool verifyChecksums     = true;
    bool lazyMode            = true;
    bool strictMode          = false;
    size_t maxMetadataSize   = kMaxMetadataSize;
    size_t maxDirectoryDepth = kMaxDirectoryDepth;
    ProgressCallback progressCb = nullptr;
};

/// Parsing result containing all decoded archive sections.
struct ParseResult {
    archive::ArchiveHeader              header;
    metadata::MetadataStore             metadata;
    filesystem::DirectoryTree           directoryTree;
    index::IndexManager                 indexManager;
    storage::ChunkManager               chunkManager;
    std::vector<uint8_t>                blocksData;
    bool                                valid = false;
    std::vector<ParseError>             warnings;
};

/// Full archive parser that walks through all sections sequentially.
///
/// The parser implements the full parsing pipeline:
/// header -> metadata -> directory tree -> index table -> chunk table -> compressed blocks -> object reconstruction
///
/// It handles malformed archives gracefully, producing rich error
/// objects with detailed information about what went wrong and where.
class Parser {
public:
    explicit Parser(ParserConfig config = {});
    ~Parser() noexcept;

    Parser(Parser&& other) noexcept;
    Parser& operator=(Parser&& other) noexcept;
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    [[nodiscard]] Result<ParseResult> parse(std::span<const uint8_t> data);
    [[nodiscard]] Result<ParseResult> parseFile(const std::string& path);

    [[nodiscard]] Result<archive::ArchiveHeader> parseHeader(std::span<const uint8_t> data);
    [[nodiscard]] Result<metadata::MetadataStore> parseMetadata(
        std::span<const uint8_t> data, const archive::ArchiveHeader& header);
    [[nodiscard]] Result<filesystem::DirectoryTree> parseDirectoryTree(
        std::span<const uint8_t> data, const archive::ArchiveHeader& header);
    [[nodiscard]] Result<index::IndexManager> parseIndexTable(
        std::span<const uint8_t> data, const archive::ArchiveHeader& header);
    [[nodiscard]] Result<storage::ChunkManager> parseChunkTable(
        std::span<const uint8_t> data, const archive::ArchiveHeader& header);

    [[nodiscard]] ParserState state() const noexcept { return state_; }
    [[nodiscard]] const std::vector<ParseError>& warnings() const noexcept { return warnings_; }

    void reset();
    void setConfig(const ParserConfig& config) { config_ = config; }
    [[nodiscard]] const ParserConfig& config() const noexcept { return config_; }

private:
    ParserConfig config_;
    ParserState state_ = ParserState::Init;
    std::vector<ParseError> warnings_;

    [[nodiscard]] ParseError makeError(ErrorCode ec, uint64_t offset, std::string msg = "");
    void addWarning(ParseError err);

    [[nodiscard]] Result<ParseResult> parseInternal(std::span<const uint8_t> data);
    [[nodiscard]] bool validateSectionBounds(
        uint64_t offset, uint64_t size, uint64_t archiveSize, uint64_t offsetForError) const;

    void parseChunk(uint8_t type, void* data);

    [[nodiscard]] const uint8_t* getSectionPointer(const std::span<const uint8_t>& data, size_t offset);
    void processSection(const std::span<const uint8_t>& data);
};

} // namespace parser
} // namespace nebula
