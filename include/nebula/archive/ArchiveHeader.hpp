#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <system_error>

namespace nebula {
namespace archive {

/// Manages reading and writing the NebulaFS archive header.
///
/// The header is the first section of any NebulaFS archive and
/// contains the magic bytes, version, offsets to all other sections,
/// and integrity checksums.
class ArchiveHeader final {
public:
    ArchiveHeader() = default;

    /// Initialize from raw binary data
    [[nodiscard]] std::error_code parse(std::span<const uint8_t> data);

    /// Serialize header to binary form
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Get the total header size
    [[nodiscard]] static constexpr size_t headerSize() noexcept {
        return sizeof(format::ArchiveHeader);
    }

    /// Validate magic bytes
    [[nodiscard]] bool isValid() const noexcept;

    /// Validate the header checksum
    [[nodiscard]] bool verifyChecksum() const;

    /// Update the header checksum
    void updateChecksum();

    /// Getters
    [[nodiscard]] uint16_t versionMajor() const noexcept { return header_.versionMajor; }
    [[nodiscard]] uint16_t versionMinor() const noexcept { return header_.versionMinor; }
    [[nodiscard]] uint16_t flags() const noexcept { return header_.flags; }
    [[nodiscard]] uint64_t archiveSize() const noexcept { return header_.archiveSize; }
    [[nodiscard]] uint64_t metadataOffset() const noexcept { return header_.metadataOffset; }
    [[nodiscard]] uint64_t directoryOffset() const noexcept { return header_.directoryOffset; }
    [[nodiscard]] uint64_t indexOffset() const noexcept { return header_.indexOffset; }
    [[nodiscard]] uint64_t chunkOffset() const noexcept { return header_.chunkOffset; }
    [[nodiscard]] uint64_t blocksOffset() const noexcept { return header_.blocksOffset; }
    [[nodiscard]] uint64_t journalOffset() const noexcept { return header_.journalOffset; }
    [[nodiscard]] uint64_t entryCount() const noexcept { return header_.entryCount; }
    [[nodiscard]] uint64_t metadataSize() const noexcept { return header_.metadataSize; }
    [[nodiscard]] uint64_t directorySize() const noexcept { return header_.directorySize; }
    [[nodiscard]] uint64_t indexSize() const noexcept { return header_.indexSize; }
    [[nodiscard]] uint64_t chunkSize() const noexcept { return header_.chunkSize; }
    [[nodiscard]] uint64_t blocksSize() const noexcept { return header_.blocksSize; }
    [[nodiscard]] uint64_t journalSize() const noexcept { return header_.journalSize; }

    /// Setters
    void setFlags(uint16_t f) noexcept { header_.flags = f; }
    void setArchiveSize(uint64_t s) noexcept { header_.archiveSize = s; }
    void setMetadataOffset(uint64_t o) noexcept { header_.metadataOffset = o; }
    void setDirectoryOffset(uint64_t o) noexcept { header_.directoryOffset = o; }
    void setIndexOffset(uint64_t o) noexcept { header_.indexOffset = o; }
    void setChunkOffset(uint64_t o) noexcept { header_.chunkOffset = o; }
    void setBlocksOffset(uint64_t o) noexcept { header_.blocksOffset = o; }
    void setJournalOffset(uint64_t o) noexcept { header_.journalOffset = o; }
    void setEntryCount(uint64_t c) noexcept { header_.entryCount = c; }
    void setMetadataSize(uint64_t s) noexcept { header_.metadataSize = s; }
    void setDirectorySize(uint64_t s) noexcept { header_.directorySize = s; }
    void setIndexSize(uint64_t s) noexcept { header_.indexSize = s; }
    void setChunkSize(uint64_t s) noexcept { header_.chunkSize = s; }
    void setBlocksSize(uint64_t s) noexcept { header_.blocksSize = s; }
    void setJournalSize(uint64_t s) noexcept { header_.journalSize = s; }

    /// Access the raw header struct
    [[nodiscard]] const format::ArchiveHeader& raw() const noexcept { return header_; }
    [[nodiscard]] format::ArchiveHeader& raw() noexcept { return header_; }

private:
    format::ArchiveHeader header_{};
};

} // namespace archive
} // namespace nebula
