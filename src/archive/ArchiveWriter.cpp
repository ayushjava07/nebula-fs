#include "nebula/archive/ArchiveWriter.hpp"
#include "nebula/utils/Checksum.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace nebula {
namespace archive {

ArchiveWriter::ArchiveWriter(WriterConfig config) : config_(config) {
    compression_ = std::make_unique<compression::CompressionEngine>(
        compression::CompressionConfig{config_.compression, config_.compressionLevel});
    if (config_.encryption != EncryptionAlgorithm::None) {
        encryption_ = std::make_unique<crypto::EncryptionEngine>(
            crypto::EncryptionConfig{config_.encryption});
    }
    chunkManager_ = std::make_unique<storage::ChunkManager>(
        storage::ChunkConfig{config_.dedupStrategy, kMinChunkSize, config_.chunkSize, 65536,
                              HashAlgorithm::SHA256, config_.enableDedup});
    indexManager_ = std::make_unique<index::IndexManager>();
    metadata_ = std::make_unique<metadata::MetadataStore>();
    directoryTree_ = std::make_unique<filesystem::DirectoryTree>();
    if (config_.enableJournal) {
        journal_ = std::make_unique<filesystem::JournalManager>();
    }
}

ArchiveWriter::~ArchiveWriter() noexcept {
    if (isOpen_ && !finalized_) {
        try { close(); } catch (...) {}
    }
}

ArchiveWriter::ArchiveWriter(ArchiveWriter&& other) noexcept
    : config_(other.config_)
    , path_(std::move(other.path_))
    , file_(std::move(other.file_))
    , header_(std::move(other.header_))
    , compression_(std::move(other.compression_))
    , encryption_(std::move(other.encryption_))
    , chunkManager_(std::move(other.chunkManager_))
    , indexManager_(std::move(other.indexManager_))
    , metadata_(std::move(other.metadata_))
    , directoryTree_(std::move(other.directoryTree_))
    , journal_(std::move(other.journal_))
    , entryCount_(other.entryCount_)
    , currentOffset_(other.currentOffset_)
    , isOpen_(other.isOpen_)
    , finalized_(other.finalized_) {
    other.isOpen_ = false;
    other.finalized_ = false;
}

ArchiveWriter& ArchiveWriter::operator=(ArchiveWriter&& other) noexcept {
    if (this != &other) {
        if (isOpen_) close();
        config_ = other.config_;
        path_ = std::move(other.path_);
        file_ = std::move(other.file_);
        header_ = std::move(other.header_);
        compression_ = std::move(other.compression_);
        encryption_ = std::move(other.encryption_);
        chunkManager_ = std::move(other.chunkManager_);
        indexManager_ = std::move(other.indexManager_);
        metadata_ = std::move(other.metadata_);
        directoryTree_ = std::move(other.directoryTree_);
        journal_ = std::move(other.journal_);
        entryCount_ = other.entryCount_;
        currentOffset_ = other.currentOffset_;
        isOpen_ = other.isOpen_;
        finalized_ = other.finalized_;
        other.isOpen_ = false;
        other.finalized_ = false;
    }
    return *this;
}

std::error_code ArchiveWriter::open(const std::string& path) {
    if (isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    path_ = path;
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return make_error_code(ErrorCode::IOError);
    }

    header_.setFlags(format::HeaderNone);
    if (config_.enableJournal) header_.setFlags(header_.flags() | format::HeaderJournaled);
    if (config_.enableDedup) header_.setFlags(header_.flags() | format::HeaderDedup);
    if (config_.compression != CompressionAlgorithm::None) header_.setFlags(header_.flags() | format::HeaderCompressed);
    if (config_.encryption != EncryptionAlgorithm::None) header_.setFlags(header_.flags() | format::HeaderEncrypted);

    currentOffset_ = ArchiveHeader::headerSize();
    header_.setArchiveSize(currentOffset_);

    file_.seekp(static_cast<std::streamoff>(currentOffset_));

    if (journal_) {
        journal_->beginCheckpoint();
    }

    isOpen_ = true;
    finalized_ = false;
    return std::error_code();
}

std::error_code ArchiveWriter::close() {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    auto ec = finalizeArchive();
    if (ec) return ec;

    file_.close();
    isOpen_ = false;
    finalized_ = true;
    return std::error_code();
}

std::error_code ArchiveWriter::addFile(const std::string& sourcePath,
                                        const std::string& archivePath) {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    std::ifstream srcFile(sourcePath, std::ios::binary | std::ios::ate);
    if (!srcFile.is_open()) {
        return make_error_code(ErrorCode::IOError);
    }

    auto fileSize = static_cast<size_t>(srcFile.tellg());
    srcFile.seekg(0);

    std::vector<uint8_t> data(fileSize);
    if (fileSize > 0) {
        srcFile.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));
        if (!srcFile) {
            return make_error_code(ErrorCode::IOError);
        }
    }

    ArchiveEntry entry;
    entry.id = entryCount_ + 1;
    entry.type = EntryType::File;
    entry.path = archivePath;
    entry.name = filesystem::FileResolver::fileName(archivePath);
    entry.originalSize = fileSize;
    entry.createdAt = Timestamp::now();
    entry.modifiedAt = entry.createdAt;
    entry.flags = EntryFlags::None;
    if (config_.compression != CompressionAlgorithm::None) {
        entry.flags = entry.flags | EntryFlags::Compressed;
    }
    if (config_.encryption != EncryptionAlgorithm::None) {
        entry.flags = entry.flags | EntryFlags::Encrypted;
    }

    return processAndWriteEntry(data, entry);
}

std::error_code ArchiveWriter::addDirectory(const std::string& archivePath,
                                             const Permissions& perms) {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    ArchiveEntry entry;
    entry.id = entryCount_ + 1;
    entry.type = EntryType::Directory;
    entry.path = archivePath;
    entry.name = filesystem::FileResolver::fileName(archivePath);
    entry.createdAt = Timestamp::now();
    entry.modifiedAt = entry.createdAt;
    entry.permissions = perms;

    auto ec = directoryTree_->insert(entry);
    if (ec) return ec;

    indexManager_->insert(entry);
    entryCount_++;

    if (journal_) {
        journal_->logCreate(entry.id, {});
    }

    reportProgress(entryCount_, entryCount_, "add_directory");
    return std::error_code();
}

std::error_code ArchiveWriter::addSymlink(const std::string& linkTarget,
                                           const std::string& archivePath) {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    std::vector<uint8_t> targetData(linkTarget.begin(), linkTarget.end());

    ArchiveEntry entry;
    entry.id = entryCount_ + 1;
    entry.type = EntryType::Symlink;
    entry.path = archivePath;
    entry.name = filesystem::FileResolver::fileName(archivePath);
    entry.originalSize = targetData.size();
    entry.createdAt = Timestamp::now();
    entry.modifiedAt = entry.createdAt;

    return processAndWriteEntry(targetData, entry);
}

std::error_code ArchiveWriter::addBlob(std::span<const uint8_t> data,
                                        const std::string& archivePath,
                                        EntryFlags flags) {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    ArchiveEntry entry;
    entry.id = entryCount_ + 1;
    entry.type = EntryType::Block;
    entry.path = archivePath;
    entry.name = filesystem::FileResolver::fileName(archivePath);
    entry.originalSize = data.size();
    entry.createdAt = Timestamp::now();
    entry.modifiedAt = entry.createdAt;
    entry.flags = flags | EntryFlags::Compressed;

    return processAndWriteEntry(data, entry);
}

std::error_code ArchiveWriter::addDirectoryTree(const std::string& sourceDir,
                                                  const std::string& archiveRoot) {
    if (!isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    if (!std::filesystem::exists(sourceDir)) {
        return make_error_code(ErrorCode::EntryNotFound);
    }

    auto ec = addDirectory(archiveRoot.empty() ? "/" : archiveRoot);
    if (ec) return ec;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
        auto relativePath = entry.path().string().substr(sourceDir.length());
        auto archiveEntryPath = archiveRoot + relativePath;

        if (entry.is_directory()) {
            ec = addDirectory(archiveEntryPath);
        } else if (entry.is_symlink()) {
            auto target = std::filesystem::read_symlink(entry.path());
            ec = addSymlink(target.string(), archiveEntryPath);
        } else if (entry.is_regular_file()) {
            ec = addFile(entry.path().string(), archiveEntryPath);
        }

        if (ec) return ec;
    }

    return std::error_code();
}

std::error_code ArchiveWriter::writeData(std::span<const uint8_t> data,
                                          const std::string& archivePath) {
    return addBlob(data, archivePath);
}

void ArchiveWriter::setMetadata(const std::string& key, const std::string& value) {
    metadata_->set(key, value);
}

void ArchiveWriter::setMetadata(const std::string& key, std::span<const uint8_t> value) {
    metadata_->set(key, value);
}

void ArchiveWriter::setEncryptionKey(std::span<const uint8_t> key) {
    if (encryption_) {
        encryption_->setKey(key);
    }
}

void ArchiveWriter::setCompression(CompressionAlgorithm algo, int level) {
    config_.compression = algo;
    config_.compressionLevel = level;
    compression_ = std::make_unique<compression::CompressionEngine>(
        compression::CompressionConfig{algo, level});
}

uint64_t ArchiveWriter::estimatedArchiveSize() const noexcept {
    return currentOffset_;
}

void ArchiveWriter::abort() {
    if (isOpen_) {
        file_.close();
        std::filesystem::remove(path_);
        isOpen_ = false;
        finalized_ = true;
    }
}

std::error_code ArchiveWriter::processAndWriteEntry(std::span<const uint8_t> data,
                                                     ArchiveEntry& entry) {
    std::vector<uint8_t> processedData(data.begin(), data.end());

    if (config_.compression != CompressionAlgorithm::None && !data.empty()) {
        auto compResult = compression_->compress(data);
        if (compResult.success) {
            processedData = std::move(compResult.data);
            entry.compression = config_.compression;
        }
    }

    if (encryption_ && config_.encryption != EncryptionAlgorithm::None && !data.empty()) {
        auto encResult = encryption_->encrypt(processedData);
        if (encResult.success) {
            processedData = std::move(encResult.data);
        }
    }

    entry.checksum = utils::ChecksumEngine::compute(data);
    entry.storedSize = processedData.size();
    entry.offset = currentOffset_;

    file_.write(reinterpret_cast<const char*>(processedData.data()),
                static_cast<std::streamsize>(processedData.size()));
    if (!file_) {
        return make_error_code(ErrorCode::IOError);
    }

    currentOffset_ += processedData.size();

    auto ec = directoryTree_->insert(entry);
    if (ec) return ec;

    indexManager_->insert(entry);
    entryCount_++;

    header_.setEntryCount(entryCount_);
    header_.setArchiveSize(currentOffset_ + ArchiveHeader::headerSize());

    if (journal_) {
        journal_->logCreate(entry.id, processedData);
    }

    reportProgress(entryCount_, entryCount_, "write_entry");
    return std::error_code();
}

std::error_code ArchiveWriter::writeHeader() {
    header_.updateChecksum();
    auto headerData = header_.serialize();
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(headerData.data()),
                static_cast<std::streamsize>(headerData.size()));
    if (!file_) {
        return make_error_code(ErrorCode::IOError);
    }
    return std::error_code();
}

std::error_code ArchiveWriter::writeMetadataSection() {
    header_.setMetadataOffset(currentOffset_);
    auto metadataData = metadata_->serialize();
    header_.setMetadataSize(metadataData.size());
    file_.write(reinterpret_cast<const char*>(metadataData.data()),
                static_cast<std::streamsize>(metadataData.size()));
    if (!file_) return make_error_code(ErrorCode::IOError);
    currentOffset_ += metadataData.size();
    return std::error_code();
}

std::error_code ArchiveWriter::writeDirectorySection() {
    header_.setDirectoryOffset(currentOffset_);
    auto dirData = directoryTree_->serialize();
    header_.setDirectorySize(dirData.size());
    file_.write(reinterpret_cast<const char*>(dirData.data()),
                static_cast<std::streamsize>(dirData.size()));
    if (!file_) return make_error_code(ErrorCode::IOError);
    currentOffset_ += dirData.size();
    return std::error_code();
}

std::error_code ArchiveWriter::writeIndexSection() {
    header_.setIndexOffset(currentOffset_);
    auto indexData = indexManager_->serialize();
    header_.setIndexSize(indexData.size());
    file_.write(reinterpret_cast<const char*>(indexData.data()),
                static_cast<std::streamsize>(indexData.size()));
    if (!file_) return make_error_code(ErrorCode::IOError);
    currentOffset_ += indexData.size();
    return std::error_code();
}

std::error_code ArchiveWriter::writeChunkSection() {
    header_.setChunkOffset(currentOffset_);
    auto chunkData = chunkManager_->serialize();
    header_.setChunkSize(chunkData.size());
    file_.write(reinterpret_cast<const char*>(chunkData.data()),
                static_cast<std::streamsize>(chunkData.size()));
    if (!file_) return make_error_code(ErrorCode::IOError);
    currentOffset_ += chunkData.size();
    return std::error_code();
}

std::error_code ArchiveWriter::writeBlocksSection() {
    header_.setBlocksOffset(currentOffset_);
    uint64_t blocksSize = 0;
    for (size_t i = 0; i < 0; ++i) {}  // placeholder
    header_.setBlocksSize(blocksSize);
    return std::error_code();
}

std::error_code ArchiveWriter::writeJournalSection() {
    if (!journal_) return std::error_code();
    header_.setJournalOffset(currentOffset_);
    { auto _ec = journal_->endCheckpoint(); (void)_ec; }
    auto journalData = journal_->serialize();
    header_.setJournalSize(journalData.size());
    file_.write(reinterpret_cast<const char*>(journalData.data()),
                static_cast<std::streamsize>(journalData.size()));
    if (!file_) return make_error_code(ErrorCode::IOError);
    currentOffset_ += journalData.size();
    return std::error_code();
}

std::error_code ArchiveWriter::finalizeArchive() {
    if (journal_) { auto _ec = journal_->commit(); (void)_ec; }

    auto ec = writeMetadataSection();
    if (ec) return ec;

    ec = writeDirectorySection();
    if (ec) return ec;

    ec = writeIndexSection();
    if (ec) return ec;

    ec = writeChunkSection();
    if (ec) return ec;

    ec = writeBlocksSection();
    if (ec) return ec;

    ec = writeJournalSection();
    if (ec) return ec;

    header_.setArchiveSize(currentOffset_);
    ec = writeHeader();
    if (ec) return ec;

    return std::error_code();
}

std::error_code ArchiveWriter::writeInternal(const uint8_t* data, size_t size) {
    // Bug #7: Memory leak - buffer allocated but never freed on error path
    auto* buffer = new uint8_t[size];
    std::memcpy(buffer, data, size);

    file_.write(reinterpret_cast<const char*>(buffer),
                static_cast<std::streamsize>(size));
    if (!file_) {
        // BUG: buffer is leaked - never deleted before returning
        return make_error_code(ErrorCode::IOError);
    }

    currentOffset_ += size;
    delete[] buffer;
    return std::error_code();
}

void ArchiveWriter::reportProgress(uint64_t current, uint64_t total, std::string_view stage) {
    if (config_.progressCb) {
        config_.progressCb(current, total, stage);
    }
}

} // namespace archive
} // namespace nebula
