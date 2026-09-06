#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace nebula {
namespace search {

struct BM25Params {
    double k1{1.2};
    double b{0.75};
};

class BM25Scorer {
public:
    explicit BM25Scorer(BM25Params params = {}) : params_(params) {}

    [[nodiscard]] double idf(size_t totalDocs, size_t docFrequency) const noexcept;

    [[nodiscard]] double scoreTerm(double idfValue,
                                   uint32_t termFreq,
                                   size_t docLength,
                                   double avgDocLength) const noexcept;

    [[nodiscard]] const BM25Params& params() const noexcept { return params_; }
    void setParams(const BM25Params& params) noexcept { params_ = params; }

private:
    BM25Params params_;
};

} // namespace search
} // namespace nebula
