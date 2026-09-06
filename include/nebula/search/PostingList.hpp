#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <span>

namespace nebula {
namespace search {

struct Posting {
    uint64_t docId{0};
    uint32_t termFrequency{0};
    std::vector<uint32_t> positions;

    bool operator==(const Posting& other) const {
        return docId == other.docId && termFrequency == other.termFrequency &&
               positions == other.positions;
    }
};

class PostingList {
public:
    PostingList() = default;

    void addOccurrence(uint64_t docId, uint32_t position);
    void addPosting(Posting posting);

    [[nodiscard]] size_t docFrequency() const noexcept { return postings_.size(); }
    [[nodiscard]] uint64_t totalTermFrequency() const noexcept { return totalTf_; }
    [[nodiscard]] const std::vector<Posting>& postings() const noexcept { return postings_; }
    [[nodiscard]] std::optional<Posting> findDoc(uint64_t docId) const;

    /// Binary delta-encoding serialization
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<PostingList> deserialize(std::span<const uint8_t> bytes);

    void clear() noexcept;

private:
    std::vector<Posting> postings_;
    uint64_t totalTf_{0};
};

} // namespace search
} // namespace nebula
