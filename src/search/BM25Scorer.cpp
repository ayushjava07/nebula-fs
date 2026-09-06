#include "nebula/search/BM25Scorer.hpp"
#include <algorithm>

namespace nebula {
namespace search {

double BM25Scorer::idf(size_t totalDocs, size_t docFrequency) const noexcept {
    if (totalDocs == 0 || docFrequency == 0) {
        return 0.0;
    }

    double N = static_cast<double>(totalDocs);
    double n = static_cast<double>(docFrequency);

    // Robertson-Spärck Jones formula with +1.0 floor to avoid negative weights
    double val = (N - n + 0.5) / (n + 0.5);
    return std::log(1.0 + std::max(0.0, val));
}

double BM25Scorer::scoreTerm(double idfValue,
                             uint32_t termFreq,
                             size_t docLength,
                             double avgDocLength) const noexcept {
    if (termFreq == 0 || idfValue <= 0.0) {
        return 0.0;
    }

    double dl = static_cast<double>(docLength);
    double avgDl = (avgDocLength > 0.0) ? avgDocLength : 1.0;

    double numerator = static_cast<double>(termFreq) * (params_.k1 + 1.0);
    double denominator = static_cast<double>(termFreq) +
        params_.k1 * (1.0 - params_.b + params_.b * (dl / avgDl));

    if (denominator <= 0.0) {
        return 0.0;
    }

    return idfValue * (numerator / denominator);
}

} // namespace search
} // namespace nebula
