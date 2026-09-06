#include "nebula/search/PostingList.hpp"
#include "nebula/utils/VarInt.hpp"

#include <algorithm>

namespace nebula {
namespace search {

void PostingList::addOccurrence(uint64_t docId, uint32_t position) {
    totalTf_++;

    if (!postings_.empty() && postings_.back().docId == docId) {
        postings_.back().termFrequency++;
        postings_.back().positions.push_back(position);
        return;
    }

    // Check if doc exists out-of-order
    auto it = std::find_if(postings_.begin(), postings_.end(),
                           [docId](const Posting& p) { return p.docId == docId; });
    if (it != postings_.end()) {
        it->termFrequency++;
        it->positions.push_back(position);
        return;
    }

    Posting p;
    p.docId = docId;
    p.termFrequency = 1;
    p.positions.push_back(position);
    postings_.push_back(std::move(p));
}

void PostingList::addPosting(Posting posting) {
    totalTf_ += posting.termFrequency;
    postings_.push_back(std::move(posting));
}

std::optional<Posting> PostingList::findDoc(uint64_t docId) const {
    for (const auto& p : postings_) {
        if (p.docId == docId) {
            return p;
        }
    }
    return std::nullopt;
}

std::vector<uint8_t> PostingList::serialize() const {
    std::vector<uint8_t> buf;
    utils::VarInt::encode(postings_.size(), buf);

    uint64_t prevDocId = 0;
    for (const auto& p : postings_) {
        uint64_t docDelta = p.docId - prevDocId;
        utils::VarInt::encode(docDelta, buf);
        prevDocId = p.docId;

        utils::VarInt::encode(p.termFrequency, buf);

        uint32_t prevPos = 0;
        for (uint32_t pos : p.positions) {
            uint32_t posDelta = pos - prevPos;
            utils::VarInt::encode(posDelta, buf);
            prevPos = pos;
        }
    }

    return buf;
}

std::optional<PostingList> PostingList::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.empty()) {
        return PostingList{};
    }

    size_t offset = 0;
    auto numDocsRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!numDocsRes.valid) return std::nullopt;
    offset += numDocsRes.consumed;

    PostingList list;
    list.postings_.reserve(static_cast<size_t>(numDocsRes.value));

    uint64_t currentDocId = 0;
    for (uint64_t i = 0; i < numDocsRes.value; ++i) {
        if (offset >= bytes.size()) return std::nullopt;

        auto deltaRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!deltaRes.valid) return std::nullopt;
        offset += deltaRes.consumed;
        currentDocId += deltaRes.value;

        if (offset >= bytes.size()) return std::nullopt;
        auto tfRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!tfRes.valid) return std::nullopt;
        offset += tfRes.consumed;

        Posting p;
        p.docId = currentDocId;
        p.termFrequency = static_cast<uint32_t>(tfRes.value);
        p.positions.reserve(p.termFrequency);

        uint32_t currentPos = 0;
        for (uint32_t j = 0; j < p.termFrequency; ++j) {
            if (offset >= bytes.size()) return std::nullopt;
            auto posDeltaRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!posDeltaRes.valid) return std::nullopt;
            offset += posDeltaRes.consumed;
            currentPos += static_cast<uint32_t>(posDeltaRes.value);
            p.positions.push_back(currentPos);
        }

        list.addPosting(std::move(p));
    }

    return list;
}

void PostingList::clear() noexcept {
    postings_.clear();
    totalTf_ = 0;
}

} // namespace search
} // namespace nebula
