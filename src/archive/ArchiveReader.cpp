#include "nebula/archive/ArchiveReader.hpp"
#include "nebula/utils/Checksum.hpp"
#include "nebula/utils/MemoryMappedFile.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace nebula {
namespace archive {

ArchiveReader::ArchiveReader(ReaderConfig config) : config_(config) {
    compression_ = std::make_unique<compression::CompressionEngine>();
    indexManager_ = std::make_unique<index::IndexManager>();
    metadata_ = std::make_unique<metadata::MetadataStore>();
    directoryTree_ = std::make_unique<filesystem::DirectoryTree>();
    chunkManager_ = std::make_unique<storage::ChunkManager>();
    parser_ = std::make_unique<parser::Parser>(
        parser::ParserConfig{config_.verifyChecksums, config_.lazyLoad, false});
}

ArchiveReader::~ArchiveReader() noexcept {
    close();
}

ArchiveReader::ArchiveReader(ArchiveReader&& other) noexcept
    : config_(other.config_)
    , path_(std::move(other.path_))
    , header_(std::move(other.header_))
    , parser_(std::move(other.parser_))
    , compression_(std::move(other.compression_))
    , decryption_(std::move(other.decryption_))
    , indexManager_(std::move(other.indexManager_))
    , metadata_(std::move(other.metadata_))
    , directoryTree_(std::move(other.directoryTree_))
    , chunkManager_(std::move(other.chunkManager_))
    , archiveData_(std::move(other.archiveData_))
    , isOpen_(other.isOpen_) {
    other.isOpen_ = false;
}

ArchiveReader& ArchiveReader::operator=(ArchiveReader&& other) noexcept {
    if (this != &other) {
        close();
        config_ = other.config_;
        path_ = std::move(other.path_);
        header_ = std::move(other.header_);
        parser_ = std::move(other.parser_);
        compression_ = std::move(other.compression_);
        decryption_ = std::move(other.decryption_);
        indexManager_ = std::move(other.indexManager_);
        metadata_ = std::move(other.metadata_);
        directoryTree_ = std::move(other.directoryTree_);
        chunkManager_ = std::move(other.chunkManager_);
        archiveData_ = std::move(other.archiveData_);
        isOpen_ = other.isOpen_;
        other.isOpen_ = false;
    }
    return *this;
}

std::error_code ArchiveReader::open(const std::string& path) {
    if (isOpen_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    path_ = path;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return make_error_code(ErrorCode::IOError);
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    archiveData_.resize(fileSize);
    if (fileSize > 0) {
        file.read(reinterpret_cast<char*>(archiveData_.data()),
                  static_cast<std::streamsize>(fileSize));
        if (!file) {
            return make_error_code(ErrorCode::IOError);
        }
    }

    file.close();

    auto ec = parseArchive();
    if (ec) return ec;

    isOpen_ = true;
    return std::error_code();
}

void ArchiveReader::close() noexcept {
    isOpen_ = false;
    archiveData_.clear();
    path_.clear();
    parser_->reset();
}

Result<std::vector<ArchiveEntry>> ArchiveReader::listEntries() const {
    if (!isOpen_) {
        return toParseError(ErrorCode::InvalidOperation, ParserState::Init, 0);
    }

    std::vector<ArchiveEntry> entries;
    const auto& indexEntries = indexManager_->entries();
    entries.reserve(indexEntries.size());

    for (const auto& idxEntry : indexEntries) {
        ArchiveEntry entry;
        entry.id = idxEntry.entryId;
        entry.offset = idxEntry.offset;
        entry.storedSize = idxEntry.size;
        entry.checksum = idxEntry.checksum;
        entries.push_back(entry);
    }

    return entries;
}

Result<ArchiveEntry> ArchiveReader::findEntry(const std::string& path) const {
    if (!isOpen_) {
        return toParseError(ErrorCode::InvalidOperation, ParserState::Init, 0);
    }

    auto dnOpt = directoryTree_->find(path);
    if (!dnOpt) {
        return toParseError(ErrorCode::EntryNotFound, ParserState::DirectoryTree, 0, path);
    }

    EntryID eid = dnOpt->id;
    auto idxOpt = indexManager_->findById(eid);
    if (!idxOpt) {
        return toParseError(ErrorCode::EntryNotFound, ParserState::IndexTable, 0, std::string("no idx for id=") + std::to_string(eid));
    }

    ArchiveEntry entry;
    entry.id = idxOpt->entryId;
    entry.offset = idxOpt->offset;
    entry.storedSize = idxOpt->size;
    entry.checksum = idxOpt->checksum;
    entry.path = path;
    entry.type = EntryType::File;
    return entry;
}

Result<ArchiveEntry> ArchiveReader::findEntry(EntryID id) const {
    if (!isOpen_) {
        return toParseError(ErrorCode::InvalidOperation, ParserState::Init, 0);
    }

    auto idxOpt = indexManager_->findById(id);
    if (!idxOpt) {
        return toParseError(ErrorCode::EntryNotFound, ParserState::IndexTable, 0,
                            "entry ID " + std::to_string(id));
    }

    ArchiveEntry entry;
    entry.id = idxOpt->entryId;
    entry.offset = idxOpt->offset;
    entry.storedSize = idxOpt->size;
    entry.checksum = idxOpt->checksum;
    return entry;
}

Result<std::vector<uint8_t>> ArchiveReader::extractEntry(const std::string& path) {
    auto entryResult = findEntry(path);
    if (isError(entryResult)) {
        return getError(entryResult);
    }
    return readEntryData(getValue(entryResult));
}

Result<std::vector<uint8_t>> ArchiveReader::extractEntry(EntryID id) {
    auto entryResult = findEntry(id);
    if (isError(entryResult)) {
        return getError(entryResult);
    }
    return readEntryData(getValue(entryResult));
}

std::error_code ArchiveReader::extractToFile(const std::string& archivePath,
                                              const std::string& outputPath) {
    auto dataResult = extractEntry(archivePath);
    if (isError(dataResult)) {
        return make_error_code(ErrorCode::EntryNotFound);
    }

    auto& data = getValue(dataResult);

    std::ofstream outFile(outputPath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        return make_error_code(ErrorCode::IOError);
    }

    outFile.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    if (!outFile) {
        return make_error_code(ErrorCode::IOError);
    }

    return std::error_code();
}

std::error_code ArchiveReader::extractAll(const std::string& outputDir) {
    auto entriesResult = listEntries();
    if (isError(entriesResult)) {
        return make_error_code(ErrorCode::IOError);
    }

    auto& entries = getValue(entriesResult);
    std::filesystem::create_directories(outputDir);

    for (size_t i = 0; i < entries.size(); ++i) {
        auto& entry = entries[i];
        auto outputPath = outputDir + "/" + std::to_string(entry.id);

        auto dataResult = readEntryData(entry);
        if (isError(dataResult)) continue;

        auto& data = getValue(dataResult);
        std::ofstream outFile(outputPath, std::ios::binary | std::ios::trunc);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<std::streamsize>(data.size()));
        }

        reportProgress(i + 1, entries.size(), "extract");
    }

    return std::error_code();
}

std::optional<std::string> ArchiveReader::getMetadata(const std::string& key) const {
    return metadata_->getString(key);
}

bool ArchiveReader::needsRecovery() const noexcept {
    return (header_.flags() & format::HeaderRecovery) != 0;
}

std::error_code ArchiveReader::recover() {
    if (!needsRecovery()) return std::error_code();
    return make_error_code(ErrorCode::NotImplemented);
}

void ArchiveReader::setDecryptionKey(std::span<const uint8_t> key) {
    if (!decryption_) {
        decryption_ = std::make_unique<crypto::EncryptionEngine>();
    }
    decryption_->setKey(key);
}

std::error_code ArchiveReader::parseArchive() {
    auto result = parser_->parse(archiveData_);
    if (isError(result)) {
        return make_error_code(ErrorCode::CorruptHeader);
    }

    auto& parseResult = getValue(result);
    header_ = std::move(parseResult.header);
    *metadata_ = std::move(parseResult.metadata);
    *directoryTree_ = std::move(parseResult.directoryTree);
    *indexManager_ = std::move(parseResult.indexManager);
    *chunkManager_ = std::move(parseResult.chunkManager);

    return std::error_code();
}

Result<std::vector<uint8_t>> ArchiveReader::readEntryData(const ArchiveEntry& entry) {
    if (entry.offset + entry.storedSize > archiveData_.size()) {
        return toParseError(ErrorCode::OutOfRange, ParserState::CompressedBlocks,
                           entry.offset);
    }

    auto dataSpan = std::span<const uint8_t>(
        archiveData_.data() + entry.offset,
        static_cast<size_t>(entry.storedSize));

    std::vector<uint8_t> result;

    bool isCompressed = (header_.flags() & format::HeaderCompressed) != 0;

    if (isCompressed && !dataSpan.empty()) {
        auto decompResult = compression_->decompress(dataSpan, 0);
        if (!decompResult.success) {
            return toParseError(ErrorCode::DecompressionError, ParserState::ObjectRecon,
                               entry.offset, std::string("decomp fail ec=") + std::to_string(decompResult.ec.value()));
        }
        result = std::move(decompResult.data);
    } else {
        result.assign(dataSpan.begin(), dataSpan.end());
    }

    if (config_.verifyChecksums) {
        auto computed = utils::ChecksumEngine::compute(result);
        if (computed != entry.checksum) {
            return toParseError(ErrorCode::ChecksumMismatch, ParserState::ObjectRecon,
                               entry.offset);
        }
    }

    return result;
}

void ArchiveReader::reportProgress(uint64_t current, uint64_t total, std::string_view stage) {
    if (config_.progressCb) {
        config_.progressCb(current, total, stage);
    }
}

} // namespace archive
} // namespace nebula
