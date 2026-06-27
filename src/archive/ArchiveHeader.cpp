#include "nebula/archive/ArchiveHeader.hpp"
#include "nebula/utils/Checksum.hpp"

#include <cstring>
#include <system_error>

namespace nebula {
namespace archive {

std::error_code ArchiveHeader::parse(std::span<const uint8_t> data) {
    if (data.size() < sizeof(format::ArchiveHeader)) {
        return make_error_code(ErrorCode::CorruptHeader);
    }

    std::memcpy(&header_, data.data(), sizeof(format::ArchiveHeader));

    if (!isValid()) {
        return make_error_code(ErrorCode::InvalidMagic);
    }

    if (!header_.isVersionSupported()) {
        return make_error_code(ErrorCode::UnsupportedVersion);
    }

    if (!verifyChecksum()) {
        return make_error_code(ErrorCode::ChecksumMismatch);
    }

    return std::error_code();
}

std::vector<uint8_t> ArchiveHeader::serialize() const {
    std::vector<uint8_t> result(sizeof(format::ArchiveHeader));
    std::memcpy(result.data(), &header_, sizeof(format::ArchiveHeader));
    return result;
}

bool ArchiveHeader::isValid() const noexcept {
    return header_.isValidMagic() && header_.isVersionSupported();
}

bool ArchiveHeader::verifyChecksum() const {
    if (header_.headerChecksum == 0) return true;

    auto serialized = serialize();
    if (serialized.size() < sizeof(format::ArchiveHeader)) return false;

    uint32_t storedChecksum = header_.headerChecksum;

    auto* mutableHeader = reinterpret_cast<format::ArchiveHeader*>(serialized.data());
    mutableHeader->headerChecksum = 0;

    uint32_t computed = utils::ChecksumEngine::crc32(serialized);
    return computed == storedChecksum;
}

void ArchiveHeader::updateChecksum() {
    auto serialized = serialize();
    auto* mutableHeader = reinterpret_cast<format::ArchiveHeader*>(serialized.data());
    mutableHeader->headerChecksum = 0;
    header_.headerChecksum = utils::ChecksumEngine::crc32(serialized);
}

} // namespace archive
} // namespace nebula
