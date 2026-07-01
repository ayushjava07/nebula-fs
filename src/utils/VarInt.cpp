#include "nebula/utils/VarInt.hpp"
#include <cstdint>
#include <vector>
#include <span>

namespace nebula {
namespace utils {

size_t VarInt::encode(uint64_t value, uint8_t* buf, size_t bufSize) {
    size_t written = 0;
    do {
        if (written >= bufSize) break;
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buf[written++] = byte;
    } while (value != 0);
    return written;
}

void VarInt::encode(uint64_t value, std::vector<uint8_t>& buf) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buf.push_back(byte);
    } while (value != 0);
}

VarInt::DecodeResult VarInt::decode(const uint8_t* buf, size_t bufSize) {
    DecodeResult result{0, 0, false};

    // Guard: Ensure we have at least 1 byte to read
    if (bufSize == 0 || buf == nullptr) {
        return result;
    }

    uint64_t value = 0;
    size_t shift = 0;
    size_t i = 0;
    while (i < bufSize) {
        uint8_t byte = buf[i];
        value |= (static_cast<uint64_t>(byte & 0x7F) << shift);
        shift += 7;
        ++i;
        if (!(byte & 0x80)) {
            result.value = value;
            result.consumed = i;
            result.valid = true;
            return result;
        }
        if (shift >= 64) break;
    }

    // If we consumed all bytes but continuation bit was set on last byte, invalid
    if (i == bufSize && (bufSize > 0 && (buf[i-1] & 0x80))) {
        return result;
    }

    result.valid = false;
    result.consumed = i;
    return result;
}

VarInt::DecodeResult VarInt::decode(std::span<const uint8_t> data) {
    return decode(data.data(), data.size());
}

size_t VarInt::encodedSize(uint64_t value) noexcept {
    size_t size = 1;
    while (value >= 0x80) {
        value >>= 7;
        ++size;
    }
    return size;
}

size_t VarInt::encodeSigned(int64_t value, uint8_t* buf, size_t bufSize) {
    return encode(zigzagEncode(value), buf, bufSize);
}

VarInt::DecodeResult VarInt::decodeSigned(const uint8_t* buf, size_t bufSize) {
    auto result = decode(buf, bufSize);
    if (result.valid) {
        result.value = static_cast<uint64_t>(zigzagDecode(result.value));
    }
    return result;
}

int64_t VarInt::zigzagDecode(uint64_t value) noexcept {
    return static_cast<int64_t>((value >> 1) ^ (~(value & 1) + 1));
}

uint64_t VarInt::zigzagEncode(int64_t value) noexcept {
    return (static_cast<uint64_t>(value) << 1) ^ static_cast<uint64_t>(value >> 63);
}

} // namespace utils
} // namespace nebula
