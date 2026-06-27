#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include <cstdint>
#include <cstddef>

namespace nebula {
namespace utils {

/// Variable-length integer encoding and decoding utilities.
///
/// Uses a unsigned LEB128-like encoding where each byte uses 7 bits
/// for data and 1 bit (MSB) to indicate continuation.
class VarInt {
public:
    /// Maximum number of bytes needed to encode a uint64_t
    static constexpr size_t kMaxEncodedSize = 10;

    /// Encode a uint64_t into a buffer
    /// Returns the number of bytes written
    static size_t encode(uint64_t value, uint8_t* buf, size_t bufSize);

    /// Encode a uint64_t and append to a vector
    static void encode(uint64_t value, std::vector<uint8_t>& buf);

    /// Decode a varint from a buffer
    /// Returns the decoded value and consumed byte count
    struct DecodeResult {
        uint64_t value;
        size_t consumed;
        bool valid;
    };
    static DecodeResult decode(const uint8_t* buf, size_t bufSize);

    /// Decode a varint from a span
    static DecodeResult decode(std::span<const uint8_t> data);

    /// Get the encoded size of a value without encoding
    static size_t encodedSize(uint64_t value) noexcept;

    /// Signed varint encoding (ZigZag)
    static size_t encodeSigned(int64_t value, uint8_t* buf, size_t bufSize);
    static DecodeResult decodeSigned(const uint8_t* buf, size_t bufSize);

    /// Convert unsigned to signed zigzag
    static int64_t zigzagDecode(uint64_t value) noexcept;
    static uint64_t zigzagEncode(int64_t value) noexcept;

private:
    VarInt() = delete;
};

} // namespace utils
} // namespace nebula
