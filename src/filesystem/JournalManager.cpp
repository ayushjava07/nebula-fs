#include "nebula/filesystem/JournalManager.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>
#include <fcntl.h>
#include <unistd.h>

namespace nebula {
namespace filesystem {

JournalManager::JournalManager(JournalConfig config) : config_(config) {
    checksum_ = std::make_unique<utils::ChecksumEngine>(HashAlgorithm::CRC32);

    if (config_.mode == JournalMode::File && !config_.journalPath.empty()) {
        journalFd_ = ::open(config_.journalPath.c_str(),
                           O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
}

JournalManager::~JournalManager() noexcept {
    if (journalFd_ >= 0) {
        ::close(journalFd_);
    }
}

JournalManager::JournalManager(JournalManager&& other) noexcept
    : config_(other.config_)
    , entries_(std::move(other.entries_))
    , sequence_(other.sequence_)
    , totalJournalSize_(other.totalJournalSize_)
    , inCheckpoint_(other.inCheckpoint_)
    , needsRecovery_(other.needsRecovery_)
    , checksum_(std::move(other.checksum_))
    , journalFd_(other.journalFd_) {
    other.journalFd_ = -1;
}

JournalManager& JournalManager::operator=(JournalManager&& other) noexcept {
    if (this != &other) {
        if (journalFd_ >= 0) ::close(journalFd_);
        config_ = other.config_;
        entries_ = std::move(other.entries_);
        sequence_ = other.sequence_;
        totalJournalSize_ = other.totalJournalSize_;
        inCheckpoint_ = other.inCheckpoint_;
        needsRecovery_ = other.needsRecovery_;
        checksum_ = std::move(other.checksum_);
        journalFd_ = other.journalFd_;
        other.journalFd_ = -1;
    }
    return *this;
}

std::error_code JournalManager::beginCheckpoint() {
    if (inCheckpoint_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    auto entry = createEntry(JournalEntryType::BeginCheckpoint, {});
    entries_.push_back(entry);
    inCheckpoint_ = true;
    needsRecovery_ = true;

    if (journalFd_ >= 0) {
        return writeToFile(entry);
    }

    return std::error_code();
}

std::error_code JournalManager::endCheckpoint() {
    if (!inCheckpoint_) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    auto entry = createEntry(JournalEntryType::EndCheckpoint, {});
    entries_.push_back(entry);
    inCheckpoint_ = false;

    if (journalFd_ >= 0) {
        return writeToFile(entry);
    }

    return std::error_code();
}

std::error_code JournalManager::log(JournalEntry entry) {
    if (isJournalFull()) {
        return make_error_code(ErrorCode::JournalFull);
    }

    if (config_.enableChecksums) {
        checksum_->reset();
        checksum_->update(reinterpret_cast<const uint8_t*>(&entry.type), sizeof(entry.type));
        checksum_->update(reinterpret_cast<const uint8_t*>(&entry.sequence), sizeof(entry.sequence));
        checksum_->update(entry.data);
        entry.checksum = checksum_->finalize();
    }

    entries_.push_back(entry);
    totalJournalSize_ += entry.data.size();

    if (journalFd_ >= 0) {
        return writeToFile(entry);
    }

    return std::error_code();
}

std::error_code JournalManager::logCreate(EntryID id, std::span<const uint8_t> data) {
    std::vector<uint8_t> entryData;
    utils::VarInt::encode(id, entryData);
    entryData.insert(entryData.end(), data.begin(), data.end());

    return log(createEntry(JournalEntryType::CreateEntry, std::move(entryData)));
}

std::error_code JournalManager::logUpdate(EntryID id, std::span<const uint8_t> data) {
    std::vector<uint8_t> entryData;
    utils::VarInt::encode(id, entryData);
    entryData.insert(entryData.end(), data.begin(), data.end());

    return log(createEntry(JournalEntryType::UpdateEntry, std::move(entryData)));
}

std::error_code JournalManager::logDelete(EntryID id) {
    std::vector<uint8_t> entryData;
    utils::VarInt::encode(id, entryData);
    return log(createEntry(JournalEntryType::DeleteEntry, std::move(entryData)));
}

std::error_code JournalManager::logWriteBlock(const ChunkDescriptor& chunk) {
    std::vector<uint8_t> entryData;
    entryData.insert(entryData.end(), chunk.hash.begin(), chunk.hash.end());
    utils::VarInt::encode(chunk.offset, entryData);
    utils::VarInt::encode(chunk.compressedSize, entryData);
    utils::VarInt::encode(chunk.originalSize, entryData);

    return log(createEntry(JournalEntryType::WriteBlock, std::move(entryData)));
}

std::error_code JournalManager::commit() {
    if (inCheckpoint_) {
        auto endEntry = createEntry(JournalEntryType::EndCheckpoint, {});
        entries_.push_back(endEntry);
        if (journalFd_ >= 0) {
            auto ec = writeToFile(endEntry);
            if (ec) return ec;
        }
        inCheckpoint_ = false;
    }

    auto entry = createEntry(JournalEntryType::Commit, {});
    entries_.push_back(entry);
    needsRecovery_ = false;

    if (journalFd_ >= 0) {
        return writeToFile(entry);
    }

    return std::error_code();
}

std::error_code JournalManager::abort() {
    auto entry = createEntry(JournalEntryType::Abort, {});
    entries_.push_back(entry);
    inCheckpoint_ = false;
    needsRecovery_ = true;

    if (journalFd_ >= 0) {
        return writeToFile(entry);
    }

    return std::error_code();
}

std::error_code JournalManager::recover() {
    if (!needsRecovery_) return std::error_code();
    needsRecovery_ = false;
    return std::error_code();
}

std::error_code JournalManager::replayEntry(const JournalEntry& entry) {
    switch (entry.type) {
        case JournalEntryType::BeginCheckpoint:
            inCheckpoint_ = true;
            break;
        case JournalEntryType::EndCheckpoint:
        case JournalEntryType::Commit:
            inCheckpoint_ = false;
            break;
        case JournalEntryType::Abort:
            inCheckpoint_ = false;
            break;
        default:
            break;
    }
    return std::error_code();
}

bool JournalManager::needsRecovery() const noexcept {
    return needsRecovery_;
}

bool JournalManager::hasUncommitted() const noexcept {
    return inCheckpoint_;
}

void JournalManager::clear() noexcept {
    entries_.clear();
    sequence_ = 0;
    totalJournalSize_ = 0;
    inCheckpoint_ = false;
    needsRecovery_ = false;
}

std::error_code JournalManager::flush() {
    if (journalFd_ >= 0) {
        if (::fsync(journalFd_) < 0) {
            return std::error_code(errno, std::generic_category());
        }
    }
    return std::error_code();
}

std::vector<uint8_t> JournalManager::serialize() const {
    std::vector<uint8_t> result;
    utils::VarInt::encode(sequence_, result);
    utils::VarInt::encode(static_cast<uint64_t>(entries_.size()), result);

    for (const auto& entry : entries_) {
        result.push_back(static_cast<uint8_t>(entry.type));
        utils::VarInt::encode(entry.sequence, result);
        size_t tsOffset = result.size();
        result.resize(result.size() + sizeof(int64_t) + sizeof(uint32_t));
        int64_t secs = entry.timestamp.seconds;
        uint32_t nanos = entry.timestamp.nanos;
        std::memcpy(result.data() + tsOffset, &secs, sizeof(int64_t));
        std::memcpy(result.data() + tsOffset + sizeof(int64_t), &nanos, sizeof(uint32_t));
        utils::VarInt::encode(static_cast<uint64_t>(entry.data.size()), result);
        result.insert(result.end(), entry.data.begin(), entry.data.end());
        result.insert(result.end(), entry.checksum.begin(), entry.checksum.end());
    }

    return result;
}

std::error_code JournalManager::deserialize(std::span<const uint8_t> data) {
    clear();

    size_t offset = 0;
    auto seqResult = utils::VarInt::decode(data.subspan(offset));
    if (!seqResult.valid) return make_error_code(ErrorCode::JournalCorrupt);
    sequence_ = seqResult.value;
    offset += seqResult.consumed;

    auto countResult = utils::VarInt::decode(data.subspan(offset));
    if (!countResult.valid) return make_error_code(ErrorCode::JournalCorrupt);
    size_t count = static_cast<size_t>(countResult.value);
    offset += countResult.consumed;

    for (size_t i = 0; i < count; ++i) {
        if (offset >= data.size()) break;

        JournalEntry entry;
        entry.type = static_cast<JournalEntryType>(data[offset++]);

        auto seqResult2 = utils::VarInt::decode(data.subspan(offset));
        if (!seqResult2.valid) break;
        entry.sequence = seqResult2.value;
        offset += seqResult2.consumed;

        if (offset + sizeof(int64_t) + sizeof(uint32_t) > data.size()) break;
        std::memcpy(&entry.timestamp.seconds, &data[offset], sizeof(int64_t));
        offset += sizeof(int64_t);
        std::memcpy(&entry.timestamp.nanos, &data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        auto dataSizeResult = utils::VarInt::decode(data.subspan(offset));
        if (!dataSizeResult.valid) break;
        size_t dataSize = static_cast<size_t>(dataSizeResult.value);
        offset += dataSizeResult.consumed;

        if (offset + dataSize + 32 > data.size()) break;
        entry.data.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                          data.begin() + static_cast<ptrdiff_t>(offset + dataSize));
        offset += dataSize;
        std::memcpy(entry.checksum.data(), &data[offset], 32);
        offset += 32;

        entries_.push_back(entry);
        totalJournalSize_ += entry.data.size();
    }

    return std::error_code();
}

bool JournalManager::verifyIntegrity() const {
    for (const auto& entry : entries_) {
        if (!checksum_) continue;
        auto expected = entry.checksum;
        checksum_->reset();
        checksum_->update(reinterpret_cast<const uint8_t*>(&entry.type), sizeof(entry.type));
        checksum_->update(reinterpret_cast<const uint8_t*>(&entry.sequence), sizeof(entry.sequence));
        checksum_->update(entry.data);
        auto computed = checksum_->finalize();
        if (computed != expected) return false;
    }
    return true;
}

JournalEntry JournalManager::createEntry(JournalEntryType type, std::vector<uint8_t> data) {
    JournalEntry entry;
    entry.type = type;
    entry.sequence = sequence_++;
    entry.timestamp = Timestamp::now();
    entry.data = std::move(data);
    return entry;
}

std::error_code JournalManager::writeToFile(const JournalEntry& entry) {
    if (journalFd_ < 0) return std::error_code();
    return std::error_code();
}

bool JournalManager::isJournalFull() const noexcept {
    return totalJournalSize_ >= config_.maxJournalSize;
}

} // namespace filesystem
} // namespace nebula
