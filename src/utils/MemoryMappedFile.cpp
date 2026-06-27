#include "nebula/utils/MemoryMappedFile.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <system_error>

namespace nebula {
namespace utils {

MemoryMappedFile::MemoryMappedFile(const std::string& path, bool readOnly)
    : readOnly_(readOnly) {
    open(path, readOnly);
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : mapHandle_(other.mapHandle_)
    , data_(other.data_)
    , size_(other.size_)
    , mapSize_(other.mapSize_)
    , path_(std::move(other.path_))
    , readOnly_(other.readOnly_) {
    other.mapHandle_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    other.mapSize_ = 0;
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        close();
        mapHandle_ = other.mapHandle_;
        data_ = other.data_;
        size_ = other.size_;
        mapSize_ = other.mapSize_;
        path_ = std::move(other.path_);
        readOnly_ = other.readOnly_;
        other.mapHandle_ = nullptr;
        other.data_ = nullptr;
        other.size_ = 0;
        other.mapSize_ = 0;
    }
    return *this;
}

MemoryMappedFile::~MemoryMappedFile() noexcept {
    close();
}

std::error_code MemoryMappedFile::open(const std::string& path, bool readOnly) {
    close();
    path_ = path;
    readOnly_ = readOnly;

    int fd = ::open(path.c_str(), readOnly ? O_RDONLY : O_RDWR);
    if (fd == -1) {
        return std::error_code(errno, std::generic_category());
    }

    struct stat st;
    if (::fstat(fd, &st) == -1) {
        int savedErrno = errno;
        ::close(fd);
        return std::error_code(savedErrno, std::generic_category());
    }

    size_ = static_cast<size_t>(st.st_size);
    if (size_ == 0) {
        ::close(fd);
        data_ = nullptr;
        mapHandle_ = nullptr;
        return std::error_code();
    }

    int prot = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
    int flags = readOnly ? MAP_PRIVATE : MAP_SHARED;

    void* mapped = ::mmap(nullptr, size_, prot, flags, fd, 0);
    if (mapped == MAP_FAILED) {
        int savedErrno = errno;
        ::close(fd);
        return std::error_code(savedErrno, std::generic_category());
    }

    ::close(fd);
    data_ = static_cast<uint8_t*>(mapped);
    mapHandle_ = mapped;
    mapSize_ = size_;

    return std::error_code();
}

void MemoryMappedFile::close() noexcept {
    if (data_ && mapHandle_) {
        ::munmap(mapHandle_, mapSize_);
    }
    mapHandle_ = nullptr;
    data_ = nullptr;
    size_ = 0;
    mapSize_ = 0;
    path_.clear();
}

std::error_code MemoryMappedFile::remap(size_t offset, size_t size) {
    if (!data_) {
        return std::error_code(ENXIO, std::generic_category());
    }
    if (offset + size > size_) {
        return std::error_code(ENOMEM, std::generic_category());
    }

    int fd = ::open(path_.c_str(), readOnly_ ? O_RDONLY : O_RDWR);
    if (fd == -1) {
        return std::error_code(errno, std::generic_category());
    }

    void* newMap = ::mmap(nullptr, size, readOnly_ ? PROT_READ : (PROT_READ | PROT_WRITE),
                          readOnly_ ? MAP_PRIVATE : MAP_SHARED,
                          fd, static_cast<off_t>(offset));
    int savedErrno = errno;
    ::close(fd);

    if (newMap == MAP_FAILED) {
        return std::error_code(savedErrno, std::generic_category());
    }

    ::munmap(mapHandle_, mapSize_);
    mapHandle_ = newMap;
    data_ = static_cast<uint8_t*>(newMap);
    mapSize_ = size;
    size_ = size;

    return std::error_code();
}

void MemoryMappedFile::advise(AccessPattern pattern) const noexcept {
    if (!data_) return;
    int advice = MADV_NORMAL;
    switch (pattern) {
        case AccessPattern::Normal:   advice = MADV_NORMAL; break;
        case AccessPattern::Sequential: advice = MADV_SEQUENTIAL; break;
        case AccessPattern::Random:   advice = MADV_RANDOM; break;
        case AccessPattern::WillNeed: advice = MADV_WILLNEED; break;
        case AccessPattern::DontNeed: advice = MADV_DONTNEED; break;
    }
    ::madvise(data_, mapSize_, advice);
}

size_t MemoryMappedFile::pageSize() noexcept {
    static const size_t pageSize = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
    return pageSize;
}

} // namespace utils
} // namespace nebula
