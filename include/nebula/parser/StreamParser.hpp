#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "Parser.hpp"

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

/// Streaming parser that processes archive data incrementally.
///
/// Unlike the bulk Parser which requires the entire archive in memory,
/// the StreamParser can process archives as data arrives, section by section.
/// This is useful for network transfers, real-time processing, and
/// memory-constrained environments.
class StreamParser {
public:
    explicit StreamParser(ParserConfig config = {});
    ~StreamParser() noexcept;

    /// Move-only
    StreamParser(StreamParser&& other) noexcept;
    StreamParser& operator=(StreamParser&& other) noexcept;
    StreamParser(const StreamParser&) = delete;
    StreamParser& operator=(const StreamParser&) = delete;

    /// Feed a chunk of data to the parser.
    /// Returns the number of bytes consumed.
    [[nodiscard]] Result<size_t> feed(std::span<const uint8_t> chunk);

    /// Check if parsing is complete.
    [[nodiscard]] bool isComplete() const noexcept { return state_ == ParserState::Complete; }

    /// Check if parser is in an error state.
    [[nodiscard]] bool hasError() const noexcept { return state_ == ParserState::Error; }

    /// Get the current parse result (only valid when complete).
    [[nodiscard]] const ParseResult& result() const noexcept { return result_; }

    /// Get current state.
    [[nodiscard]] ParserState state() const noexcept { return state_; }

    /// Get warnings.
    [[nodiscard]] const std::vector<ParseError>& warnings() const noexcept { return warnings_; }

    /// Reset for reuse.
    void reset();

    /// Get the progress as a fraction [0.0, 1.0].
    [[nodiscard]] double progress() const noexcept;

    /// Get bytes processed so far.
    [[nodiscard]] size_t bytesProcessed() const noexcept { return bytesProcessed_; }

private:
    ParserConfig config_;
    ParserState state_ = ParserState::Init;
    ParseResult result_;
    std::vector<uint8_t> buffer_;
    std::vector<ParseError> warnings_;
    size_t bytesProcessed_ = 0;
    size_t expectedSize_ = 0;

    [[nodiscard]] Result<size_t> processHeader();
    [[nodiscard]] Result<size_t> processMetadata();
    [[nodiscard]] Result<size_t> processDirectoryTree();
    [[nodiscard]] Result<size_t> processIndexTable();
    [[nodiscard]] Result<size_t> processChunkTable();
    [[nodiscard]] Result<size_t> processCompressedBlocks();
    [[nodiscard]] Result<size_t> finalize();

    [[nodiscard]] bool haveBytes(size_t needed) const noexcept;
    void transitionTo(ParserState newState);
};

} // namespace parser
} // namespace nebula
