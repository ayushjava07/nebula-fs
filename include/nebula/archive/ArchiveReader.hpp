#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "../compression/CompressionEngine.hpp"
#include "../crypto/EncryptionEngine.hpp"
#include "../storage/ChunkManager.hpp"
#include "../index/IndexManager.hpp"
#include "../metadata/MetadataStore.hpp"
#include "../filesystem/DirectoryTree.hpp"
#include "../parser/Parser.hpp"
#include "ArchiveHeader.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>
#include <functional>

namespace nebula {
namespace archive {

/// Read mode for extraction.
enum class ExtractMode : uint8_t {
    Exact,      ///< Restore everything as stored
    Decompress, ///< Decompress but keep encryption
    Decrypt,    ///< Decrypt but keep compression
    FullRestore ///< Fully decompress and decrypt
};

/// Configuration for the archive reader.
struct ReaderConfig {
    bool verifyChecksums       = true;
    bool lazyLoad              = true;
    bool enableCache           = true;
    size_t cacheSize           = kBTreeCacheSize;
    ExtractMode extractMode    = ExtractMode::FullRestore;
    ProgressCallback progressCb = nullptr;
};

/// High-level archive reader.
///
/// Provides random access to archive entries with lazy loading,
/// caching, and transparent decompression/decryption.
class ArchiveReader {
public:
    explicit ArchiveReader(ReaderConfig config = {});
    ~ArchiveReader() noexcept;

    /// Move-only
    ArchiveReader(ArchiveReader&& other) noexcept;
    ArchiveReader& operator=(ArchiveReader&& other) noexcept;
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;

    /// Open an existing archive.
    [[nodiscard]] std::error_code open(const std::string& path);

    /// Close the archive.
    void close() noexcept;

    /// Check if archive is open.
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    /// List all entries in the archive.
    [[nodiscard]] Result<std::vector<ArchiveEntry>> listEntries() const;

    /// Find an entry by path.
    [[nodiscard]] Result<ArchiveEntry> findEntry(const std::string& path) const;

    /// Find an entry by ID.
    [[nodiscard]] Result<ArchiveEntry> findEntry(EntryID id) const;

    /// Extract an entry's data.
    [[nodiscard]] Result<std::vector<uint8_t>> extractEntry(const std::string& path);

    /// Extract an entry by ID.
    [[nodiscard]] Result<std::vector<uint8_t>> extractEntry(EntryID id);

    /// Extract to a file.
    [[nodiscard]] std::error_code extractToFile(const std::string& archivePath,
                                                  const std::string& outputPath);

    /// Extract all entries to a directory.
    [[nodiscard]] std::error_code extractAll(const std::string& outputDir);

    /// Get entry count.
    [[nodiscard]] size_t entryCount() const noexcept { return header_.entryCount(); }

    /// Get archive metadata.
    [[nodiscard]] std::optional<std::string> getMetadata(const std::string& key) const;

    /// Get all metadata.
    [[nodiscard]] const metadata::MetadataStore& metadataStore() const { return *metadata_; }

    /// Check if archive needs recovery.
    [[nodiscard]] bool needsRecovery() const noexcept;

    /// Attempt recovery.
    [[nodiscard]] std::error_code recover();

    /// Set decryption key.
    void setDecryptionKey(std::span<const uint8_t> key);

    /// Get the archive header.
    [[nodiscard]] const ArchiveHeader& header() const noexcept { return header_; }

    /// Get the directory tree.
    [[nodiscard]] const filesystem::DirectoryTree& directoryTree() const { return *directoryTree_; }

    /// Get the index manager.
    [[nodiscard]] const index::IndexManager& indexManager() const { return *indexManager_; }

private:
    ReaderConfig config_;
    std::string path_;
    ArchiveHeader header_;
    std::unique_ptr<parser::Parser> parser_;
    std::unique_ptr<compression::CompressionEngine> compression_;
    std::unique_ptr<crypto::EncryptionEngine> decryption_;
    std::unique_ptr<index::IndexManager> indexManager_;
    std::unique_ptr<metadata::MetadataStore> metadata_;
    std::unique_ptr<filesystem::DirectoryTree> directoryTree_;
    std::unique_ptr<storage::ChunkManager> chunkManager_;
    std::vector<uint8_t> archiveData_;
    bool isOpen_ = false;

    [[nodiscard]] std::error_code parseArchive();
    [[nodiscard]] Result<std::vector<uint8_t>> readEntryData(const ArchiveEntry& entry);
    void reportProgress(uint64_t current, uint64_t total, std::string_view stage);
};

} // namespace archive
} // namespace nebula
