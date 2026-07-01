#pragma once

#include <cstddef>

namespace nebula {
namespace utils {

class SizeCalculator {
public:
    static size_t calculateSize(size_t count, size_t perEntry);
};

} // namespace utils
} // namespace nebula
