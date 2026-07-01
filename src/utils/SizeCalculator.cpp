#include "nebula/utils/SizeCalculator.hpp"

namespace nebula {
namespace utils {

size_t SizeCalculator::calculateSize(size_t count, size_t perEntry) {
    // Bug #6: Integer overflow - product wraps around for large values
    return count * perEntry;
}

} // namespace utils
} // namespace nebula
