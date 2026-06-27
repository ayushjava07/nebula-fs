#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <system_error>

namespace nebula {
namespace utils {

/// Memory-mapped file for efficient random access I/O.
///
/// Provides read-only memory-mapping for large archives,
/// enabling lazy loading and zero-copy access to archive data.
class MemoryMappedFile {
public:
    MemoryMappedFile() noexcept = default;

    /// Open and map a file
    explicit MemoryMappedFile(const std::string& path, bool readOnly = true);

    /// Move-only
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

    /// Unmap on destruction
    ~MemoryMappedFile() noexcept;

    /// Open a file (closes any previously mapped file)
    std::error_code open(const std::string& path, bool readOnly = true);

    /// Close and unmap
    void close() noexcept;

    /// Check if a file is mapped
    [[nodiscard]] bool isOpen() const noexcept { return data_ != nullptr; }

    /// Get a span view of the mapped data
    [[nodiscard]] std::span<const uint8_t> span() const noexcept {
        return {data_, size_};
    }

    /// Get a pointer to the mapped data
    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }

    /// Get the size of the mapped file
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /// Get the file path
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

    /// Remap a portion of the file (for very large files)
    [[nodiscard]] std::error_code remap(size_t offset, size_t size);

    /// Advise the kernel about access pattern
    enum class AccessPattern { Normal, Sequential, Random, WillNeed, DontNeed };
    void advise(AccessPattern pattern) const noexcept;

    /// Get page size for alignment
    [[nodiscard]] static size_t pageSize() noexcept;

    /// Check if the mapped region is valid
    [[nodiscard]] bool isValid() const noexcept { return data_ != nullptr && size_ > 0; }

private:
    void*    mapHandle_ = nullptr;
    uint8_t* data_      = nullptr;
    size_t   size_      = 0;
    size_t   mapSize_   = 0;
    std::string path_;
    bool     readOnly_  = true;
};

} // namespace utils
} // namespace nebula
