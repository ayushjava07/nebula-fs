/// Utility to generate seed corpus files for fuzz targets.
/// Compile: g++ -std=c++20 generate_seeds.cpp -o gen_seeds && ./gen_seeds

#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <filesystem>

#pragma pack(push, 1)
struct ArchiveHeader {
    uint8_t  magic[4]      = {'N', 'B', 'F', 0x01};
    uint16_t versionMajor   = 1;
    uint16_t versionMinor   = 0;
    uint16_t flags          = 0;
    uint64_t archiveSize    = 0;
    uint64_t metadataOffset = 0;
    uint64_t directoryOffset= 0;
    uint64_t indexOffset    = 0;
    uint64_t chunkOffset    = 0;
    uint64_t blocksOffset   = 0;
    uint64_t journalOffset  = 0;
    uint64_t entryCount     = 0;
    uint64_t metadataSize   = 0;
    uint64_t directorySize  = 0;
    uint64_t indexSize      = 0;
    uint64_t chunkSize      = 0;
    uint64_t blocksSize     = 0;
    uint64_t journalSize    = 0;
    uint32_t headerChecksum = 0;
    uint8_t  archiveChecksum[32] = {};
};
#pragma pack(pop)
static_assert(sizeof(ArchiveHeader) == 158, "ArchiveHeader size mismatch");

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    }
    return ~crc;
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<uint8_t> createValidArchive() {
    ArchiveHeader header;
    header.archiveSize = sizeof(ArchiveHeader);
    auto hdrBytes = reinterpret_cast<const uint8_t*>(&header);
    header.headerChecksum = crc32(hdrBytes + 8, sizeof(ArchiveHeader) - 8);
    std::vector<uint8_t> result(sizeof(ArchiveHeader));
    std::memcpy(result.data(), &header, sizeof(ArchiveHeader));
    return result;
}

std::vector<uint8_t> createArchiveWithMetadata() {
    auto archive = createValidArchive();
    auto* hdr = reinterpret_cast<ArchiveHeader*>(archive.data());
    std::vector<uint8_t> metadata;
    metadata.push_back(4); // key len
    metadata.push_back(5); // val len
    metadata.insert(metadata.end(), {'t', 'e', 's', 't'});
    metadata.insert(metadata.end(), {'v', 'a', 'l', 'u', 'e'});
    hdr->metadataOffset = sizeof(ArchiveHeader);
    hdr->metadataSize = metadata.size();
    hdr->archiveSize = sizeof(ArchiveHeader) + metadata.size();
    archive.insert(archive.end(), metadata.begin(), metadata.end());
    return archive;
}

std::vector<uint8_t> createValidJournal() {
    std::vector<uint8_t> journal;
    journal.push_back(0);                          // sequence varint
    journal.push_back(2);                          // entry count varint
    // Entry 1: BeginCheckpoint
    journal.push_back(0x01); journal.push_back(0); // type, sequence
    for (int i = 0; i < 12; ++i) journal.push_back(0); // timestamp
    journal.push_back(0);                          // data size varint
    journal.insert(journal.end(), 32, 0);          // checksum
    // Entry 2: Commit
    journal.push_back(0x08); journal.push_back(0);
    for (int i = 0; i < 12; ++i) journal.push_back(0);
    journal.push_back(0);
    journal.insert(journal.end(), 32, 0);
    return journal;
}

int main() {
    std::filesystem::create_directories("archive_parser_fuzzer");
    std::filesystem::create_directories("index_fuzzer");
    std::filesystem::create_directories("compression_fuzzer");
    std::filesystem::create_directories("journal_fuzzer");
    std::filesystem::create_directories("metadata_fuzzer");

    writeFile("archive_parser_fuzzer/seed_empty.nbf", createValidArchive());
    writeFile("archive_parser_fuzzer/seed_with_meta.nbf", createArchiveWithMetadata());
    writeFile("archive_parser_fuzzer/seed_header_only.nbf", createValidArchive());

    std::vector<uint8_t> emptyIndex = {0};
    writeFile("index_fuzzer/seed_empty.idx", emptyIndex);
    std::vector<uint8_t> smallIndex;
    smallIndex.push_back(1); // 1 entry
    smallIndex.push_back(1); // entryId = 1
    smallIndex.push_back(158); // offset
    smallIndex.push_back(19);  // size
    smallIndex.insert(smallIndex.end(), 32, 0); // checksum
    writeFile("index_fuzzer/seed_small.idx", smallIndex);

    std::vector<uint8_t> compressible(100, 'A');
    writeFile("compression_fuzzer/seed_repeating.bin", compressible);
    std::vector<uint8_t> randomData;
    for (int i = 0; i < 100; ++i) randomData.push_back(static_cast<uint8_t>(i));
    writeFile("compression_fuzzer/seed_random.bin", randomData);

    writeFile("journal_fuzzer/seed_valid.jnl", createValidJournal());

    std::vector<uint8_t> metaEntry;
    metaEntry.push_back(4); metaEntry.push_back(5);
    metaEntry.insert(metaEntry.end(), {'t', 'e', 's', 't'});
    metaEntry.insert(metaEntry.end(), {'v', 'a', 'l', 'u', 'e'});
    writeFile("metadata_fuzzer/seed_valid.meta", metaEntry);
    writeFile("metadata_fuzzer/seed_empty.meta", std::vector<uint8_t>{});

    // Copy all seed directories to ../corpus/ for CFL auto-detection
    std::filesystem::create_directories("../corpus");
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (entry.is_directory()) {
            auto target = "../corpus/" + entry.path().filename().string();
            std::filesystem::remove_all(target, ec);
            std::filesystem::copy(entry.path(), target,
                std::filesystem::copy_options::recursive, ec);
        }
    }

    std::cout << "Seed corpus files generated in per-harness dirs and ../corpus/\n";
    return 0;
}
