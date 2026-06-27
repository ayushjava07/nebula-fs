#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <span>
#include <stdexcept>
#include <sstream>

namespace nebula {
namespace utils {

/// A dynamically-growing buffer for serialization and deserialization.
///
/// Provides safe read/write operations with bounds checking.
/// Supports growing on demand and zero-copy read via spans.
class Buffer {
public:
    explicit Buffer(size_t initialCapacity = kDefaultBufferSize);
    Buffer(const uint8_t* data, size_t length);
    Buffer(std::span<const uint8_t> data);
    Buffer(const Buffer& other);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer& other);
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer() noexcept = default;

    /// Write operations with bounds checking
    void write(uint8_t byte);
    void write(const uint8_t* data, size_t length);
    void write(std::span<const uint8_t> data);
    void writeAt(size_t pos, const uint8_t* data, size_t length);

    /// Write integral types in little-endian order
    template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    void writeLE(T value) {
        uint8_t buf[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); ++i) {
            buf[i] = static_cast<uint8_t>(value >> (i * 8));
        }
        write(buf, sizeof(T));
    }

    /// Read operations with bounds checking
    [[nodiscard]] uint8_t readByte(size_t pos) const;
    void read(size_t pos, uint8_t* data, size_t length) const;
    void read(size_t pos, std::span<uint8_t> data) const;

    /// Read integral types in little-endian order
    template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    [[nodiscard]] T readLE(size_t pos) const {
        if (pos + sizeof(T) > data_.size()) {
            throw std::out_of_range("Buffer::readLE: out of range");
        }
        T value = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(data_[pos + i]) << (i * 8);
        }
        return value;
    }

    /// Read a variable-length integer at position pos, advancing posRef
    [[nodiscard]] uint64_t readVarInt(size_t pos, size_t& advanced) const;

    /// Write a variable-length integer
    void writeVarInt(uint64_t value);

    /// View operations (no copy)
    [[nodiscard]] std::span<const uint8_t> view(size_t pos, size_t length) const;
    [[nodiscard]] std::span<uint8_t> mutableView(size_t pos, size_t length);

    /// Access raw data
    [[nodiscard]] const uint8_t* data() const noexcept { return data_.data(); }
    [[nodiscard]] uint8_t* data() noexcept { return data_.data(); }
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    /// Reserve capacity
    void reserve(size_t capacity);

    /// Clear the buffer
    void clear() noexcept;

    /// Shrink to fit
    void shrinkToFit();

    /// Swap contents
    void swap(Buffer& other) noexcept;

    /// Slice a portion of the buffer into a new buffer
    [[nodiscard]] Buffer slice(size_t pos, size_t length) const;

    /// Append another buffer
    void append(const Buffer& other);

    /// Comparison
    [[nodiscard]] bool operator==(const Buffer& other) const noexcept;
    [[nodiscard]] bool operator!=(const Buffer& other) const noexcept;

    /// Conversion to string (for text content)
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] std::string toHexString() const;

private:
    std::vector<uint8_t> data_;
    void checkBounds(size_t pos, size_t length) const;
    void growIfNeeded(size_t additional);
};

} // namespace utils
} // namespace nebula
