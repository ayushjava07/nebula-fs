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
#include "../filesystem/FileResolver.hpp"
#include "../filesystem/JournalManager.hpp"
#include "ArchiveHeader.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>
#include <filesystem>
#include <functional>
#include <fstream>

namespace nebula {
namespace archive {

/// Configuration for the archive writer.
struct WriterConfig {
    CompressionAlgorithm compression = CompressionAlgorithm::Zstd;
    int compressionLevel           = 3;
    EncryptionAlgorithm encryption = EncryptionAlgorithm::None;
    bool enableDedup               = true;
    bool enableJournal             = true;
    bool enableChecksum            = true;
    DedupStrategy dedupStrategy    = DedupStrategy::Content;
    size_t blockSize               = kDefaultBlockSize;
    size_t chunkSize               = kMaxChunkSize;
    ProgressCallback progressCb    = nullptr;
};

/// High-level archive writer.
///
/// Creates NebulaFS archives by orchestrating all lower-level
/// components: compression, encryption, chunking, indexing,
/// metadata management, and journaling.
class ArchiveWriter {
public:
    explicit ArchiveWriter(WriterConfig config = {});
    ~ArchiveWriter() noexcept;

    /// Move-only
    ArchiveWriter(ArchiveWriter&& other) noexcept;
    ArchiveWriter& operator=(ArchiveWriter&& other) noexcept;
    ArchiveWriter(const ArchiveWriter&) = delete;
    ArchiveWriter& operator=(const ArchiveWriter&) = delete;

    /// Open a new archive file for writing.
    [[nodiscard]] std::error_code open(const std::string& path);

    /// Close the archive, finalizing all sections.
    [[nodiscard]] std::error_code close();

    /// Add a file from the filesystem.
    [[nodiscard]] std::error_code addFile(const std::string& sourcePath,
                                           const std::string& archivePath);

    /// Add a directory entry.
    [[nodiscard]] std::error_code addDirectory(const std::string& archivePath,
                                                const Permissions& perms = {});

    /// Add a symbolic link.
    [[nodiscard]] std::error_code addSymlink(const std::string& linkTarget,
                                              const std::string& archivePath);

    /// Add a binary blob from memory.
    [[nodiscard]] std::error_code addBlob(std::span<const uint8_t> data,
                                           const std::string& archivePath,
                                           EntryFlags flags = EntryFlags::None);

    /// Add a directory tree recursively.
    [[nodiscard]] std::error_code addDirectoryTree(const std::string& sourceDir,
                                                    const std::string& archiveRoot = "");

    /// Write raw data directly (for streaming).
    [[nodiscard]] std::error_code writeData(std::span<const uint8_t> data,
                                             const std::string& archivePath);

    /// Set archive metadata.
    void setMetadata(const std::string& key, const std::string& value);
    void setMetadata(const std::string& key, std::span<const uint8_t> value);

    /// Set the encryption key.
    void setEncryptionKey(std::span<const uint8_t> key);

    /// Enable or disable compression.
    void setCompression(CompressionAlgorithm algo, int level = 3);

    /// Get the current archive size estimate.
    [[nodiscard]] uint64_t estimatedArchiveSize() const noexcept;

    /// Get the number of entries written.
    [[nodiscard]] size_t entryCount() const noexcept { return entryCount_; }

    /// Abort the current write (closes without finalizing).
    void abort();

private:
    WriterConfig config_;
    std::string path_;
    std::ofstream file_;
    ArchiveHeader header_;
    std::unique_ptr<compression::CompressionEngine> compression_;
    std::unique_ptr<crypto::EncryptionEngine> encryption_;
    std::unique_ptr<storage::ChunkManager> chunkManager_;
    std::unique_ptr<index::IndexManager> indexManager_;
    std::unique_ptr<metadata::MetadataStore> metadata_;
    std::unique_ptr<filesystem::DirectoryTree> directoryTree_;
    std::unique_ptr<filesystem::JournalManager> journal_;
    size_t entryCount_ = 0;
    uint64_t currentOffset_ = 0;
    bool isOpen_ = false;
    bool finalized_ = false;

    [[nodiscard]] std::error_code writeHeader();
    [[nodiscard]] std::error_code writeMetadataSection();
    [[nodiscard]] std::error_code writeDirectorySection();
    [[nodiscard]] std::error_code writeIndexSection();
    [[nodiscard]] std::error_code writeChunkSection();
    [[nodiscard]] std::error_code writeBlocksSection();
    [[nodiscard]] std::error_code writeJournalSection();
    [[nodiscard]] std::error_code finalizeArchive();

    [[nodiscard]] std::error_code processAndWriteEntry(
        std::span<const uint8_t> data,
        ArchiveEntry& entry);

    void reportProgress(uint64_t current, uint64_t total, std::string_view stage);
};

} // namespace archive
} // namespace nebula
