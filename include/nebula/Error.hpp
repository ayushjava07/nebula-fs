#pragma once

#include "Types.hpp"
#include <string>
#include <system_error>
#include <iostream>
#include <sstream>
#include <utility>

namespace nebula {

/// Error codes for the NebulaFS library
enum class ErrorCode : int {
    Success                = 0,
    InvalidMagic           = 1,
    UnsupportedVersion     = 2,
    CorruptHeader          = 3,
    CorruptMetadata        = 4,
    CorruptDirectory       = 5,
    CorruptIndex           = 6,
    CorruptChunkTable      = 7,
    CorruptBlock           = 8,
    ChecksumMismatch       = 9,
    DecompressionError     = 10,
    EncryptionError        = 11,
    EntryNotFound          = 12,
    DuplicateEntry         = 13,
    InvalidPath            = 14,
    NameTooLong            = 15,
    DepthExceeded          = 16,
    OutOfRange             = 17,
    IOError                = 18,
    NoSpace                = 19,
    NotAnArchive           = 20,
    ArchiveTooLarge        = 21,
    JournalCorrupt         = 22,
    JournalFull            = 23,
    RecoveryRequired       = 24,
    InvalidOperation       = 25,
    NotImplemented         = 26,
    VersionMismatch        = 27,
    EncryptionRequired     = 28,
    AuthenticationFailed   = 29,
    InsufficientPermissions = 30,
    InternalError          = 31
};

/// Custom error category for NebulaFS errors
class NebulaErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "NebulaFS";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<ErrorCode>(ev)) {
            case ErrorCode::Success:             return "success";
            case ErrorCode::InvalidMagic:        return "invalid magic bytes";
            case ErrorCode::UnsupportedVersion:  return "unsupported archive version";
            case ErrorCode::CorruptHeader:       return "corrupt archive header";
            case ErrorCode::CorruptMetadata:     return "corrupt metadata section";
            case ErrorCode::CorruptDirectory:    return "corrupt directory tree";
            case ErrorCode::CorruptIndex:        return "corrupt index table";
            case ErrorCode::CorruptChunkTable:   return "corrupt chunk table";
            case ErrorCode::CorruptBlock:        return "corrupt compressed block";
            case ErrorCode::ChecksumMismatch:    return "checksum mismatch";
            case ErrorCode::DecompressionError:  return "decompression failure";
            case ErrorCode::EncryptionError:     return "encryption/decryption failure";
            case ErrorCode::EntryNotFound:       return "entry not found";
            case ErrorCode::DuplicateEntry:      return "duplicate entry";
            case ErrorCode::InvalidPath:         return "invalid path";
            case ErrorCode::NameTooLong:         return "filename too long";
            case ErrorCode::DepthExceeded:       return "directory depth exceeded";
            case ErrorCode::OutOfRange:          return "offset out of range";
            case ErrorCode::IOError:             return "I/O error";
            case ErrorCode::NoSpace:             return "no space left";
            case ErrorCode::NotAnArchive:        return "not a NebulaFS archive";
            case ErrorCode::ArchiveTooLarge:     return "archive exceeds size limit";
            case ErrorCode::JournalCorrupt:      return "journal is corrupt";
            case ErrorCode::JournalFull:         return "journal is full";
            case ErrorCode::RecoveryRequired:    return "recovery required before use";
            case ErrorCode::InvalidOperation:    return "invalid operation for current mode";
            case ErrorCode::NotImplemented:      return "feature not implemented";
            case ErrorCode::VersionMismatch:     return "version mismatch";
            case ErrorCode::EncryptionRequired:  return "encryption required";
            case ErrorCode::AuthenticationFailed:return "authentication failed";
            case ErrorCode::InsufficientPermissions: return "insufficient permissions";
            case ErrorCode::InternalError:       return "internal error";
            default:                             return "unknown error";
        }
    }

    static const std::error_category& get() noexcept {
        static const NebulaErrorCategory instance;
        return instance;
    }
};

/// Convert ErrorCode to std::error_code
inline std::error_code make_error_code(ErrorCode ec) noexcept {
    return std::error_code(static_cast<int>(ec), NebulaErrorCategory::get());
}

/// Register ErrorCode with std::error_code
namespace detail {
    struct ErrorCodeRegister {
        ErrorCodeRegister() {
            std::error_code(static_cast<int>(ErrorCode::Success), NebulaErrorCategory::get());
        }
    };
} // namespace detail

/// Exception class for NebulaFS errors
class NebulaException : public std::system_error {
public:
    explicit NebulaException(ErrorCode ec, std::string what = "")
        : std::system_error(make_error_code(ec), std::move(what)) {}
    NebulaException(ErrorCode ec, std::string_view context, std::string detail)
        : std::system_error(make_error_code(ec),
            std::string(context) + ": " + std::move(detail)) {}
};

/// Helper to create a ParseError from an ErrorCode
inline ParseError toParseError(ErrorCode ec, ParserState state, uint64_t offset,
    std::string message = "")
{
    ParseError err;
    err.state = state;
    err.offset = offset;
    err.message = message.empty() ? NebulaErrorCategory::get().message(static_cast<int>(ec)) : message;
    err.severity = (ec == ErrorCode::CorruptHeader || ec == ErrorCode::ChecksumMismatch)
        ? ErrorSeverity::Fatal : ErrorSeverity::Recoverable;
    return err;
}

/// Check if a Result contains an error
template<typename T>
[[nodiscard]] inline bool isError(const Result<T>& result) noexcept {
    return std::holds_alternative<ParseError>(result);
}

/// Extract the value from a Result (undefined behavior if it's an error)
template<typename T>
[[nodiscard]] inline T& getValue(Result<T>& result) {
    return std::get<T>(result);
}

template<typename T>
[[nodiscard]] inline const T& getValue(const Result<T>& result) {
    return std::get<T>(result);
}

template<typename T>
[[nodiscard]] inline T&& getValue(Result<T>&& result) {
    return std::move(std::get<T>(result));
}

/// Extract the error from a Result
template<typename T>
[[nodiscard]] inline ParseError& getError(Result<T>& result) {
    return std::get<ParseError>(result);
}

template<typename T>
[[nodiscard]] inline const ParseError& getError(const Result<T>& result) {
    return std::get<ParseError>(result);
}

} // namespace nebula

namespace std {
    template<>
    struct is_error_code_enum<nebula::ErrorCode> : true_type {};
} // namespace std
