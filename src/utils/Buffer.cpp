#include "nebula/utils/Buffer.hpp"
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace nebula {
namespace utils {

Buffer::Buffer(size_t initialCapacity) {
    data_.reserve(initialCapacity);
}

Buffer::Buffer(const uint8_t* data, size_t length) {
    data_.assign(data, data + length);
}

Buffer::Buffer(std::span<const uint8_t> data) {
    data_.assign(data.begin(), data.end());
}

Buffer::Buffer(const Buffer& other) : data_(other.data_) {}

Buffer::Buffer(Buffer&& other) noexcept : data_(std::move(other.data_)) {}

Buffer& Buffer::operator=(const Buffer& other) {
    if (this != &other) {
        data_ = other.data_;
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
    }
    return *this;
}

void Buffer::write(uint8_t byte) {
    data_.push_back(byte);
}

void Buffer::write(const uint8_t* data, size_t length) {
    if (length == 0) return;
    growIfNeeded(length);
    data_.insert(data_.end(), data, data + length);
}

void Buffer::write(std::span<const uint8_t> data) {
    write(data.data(), data.size());
}

void Buffer::writeAt(size_t pos, const uint8_t* data, size_t length) {
    checkBounds(pos, length);
    std::memcpy(data_.data() + pos, data, length);
}

uint8_t Buffer::readByte(size_t pos) const {
    checkBounds(pos, 1);
    return data_[pos];
}

void Buffer::read(size_t pos, uint8_t* data, size_t length) const {
    checkBounds(pos, length);
    std::memcpy(data, data_.data() + pos, length);
}

void Buffer::read(size_t pos, std::span<uint8_t> data) const {
    read(pos, data.data(), data.size());
}

uint64_t Buffer::readVarInt(size_t pos, size_t& advanced) const {
    uint64_t value = 0;
    size_t shift = 0;
    size_t i = pos;
    while (i < data_.size()) {
        uint8_t byte = data_[i];
        value |= (static_cast<uint64_t>(byte & 0x7F) << shift);
        shift += 7;
        ++i;
        if (!(byte & 0x80)) {
            advanced = i - pos;
            return value;
        }
        if (shift >= 64) break;
    }
    advanced = i - pos;
    return value;
}

void Buffer::writeVarInt(uint64_t value) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        data_.push_back(byte);
    } while (value != 0);
}

std::span<const uint8_t> Buffer::view(size_t pos, size_t length) const {
    checkBounds(pos, length);
    return std::span<const uint8_t>(data_.data() + pos, length);
}

std::span<uint8_t> Buffer::mutableView(size_t pos, size_t length) {
    checkBounds(pos, length);
    return std::span<uint8_t>(data_.data() + pos, length);
}

void Buffer::reserve(size_t capacity) {
    data_.reserve(capacity);
}

void Buffer::clear() noexcept {
    data_.clear();
}

void Buffer::shrinkToFit() {
    data_.shrink_to_fit();
}

void Buffer::swap(Buffer& other) noexcept {
    data_.swap(other.data_);
}

Buffer Buffer::slice(size_t pos, size_t length) const {
    checkBounds(pos, length);
    return Buffer(data_.data() + pos, length);
}

void Buffer::append(const Buffer& other) {
    write(other.data_.data(), other.data_.size());
}

bool Buffer::operator==(const Buffer& other) const noexcept {
    return data_ == other.data_;
}

bool Buffer::operator!=(const Buffer& other) const noexcept {
    return data_ != other.data_;
}

std::string Buffer::toString() const {
    return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
}

std::string Buffer::toHexString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

void Buffer::checkBounds(size_t pos, size_t length) const {
    if (pos + length > data_.size()) {
        std::ostringstream oss;
        oss << "Buffer::checkBounds: pos=" << pos << " length=" << length
            << " size=" << data_.size();
        throw std::out_of_range(oss.str());
    }
}

void Buffer::growIfNeeded(size_t additional) {
    if (data_.capacity() < data_.size() + additional) {
        data_.reserve(std::max(data_.capacity() * 2, data_.size() + additional));
    }
}

} // namespace utils
} // namespace nebula
