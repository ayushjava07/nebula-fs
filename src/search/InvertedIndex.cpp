#include "nebula/search/InvertedIndex.hpp"
#include "nebula/utils/VarInt.hpp"

#include <algorithm>
#include <mutex>

namespace nebula {
namespace search {

InvertedIndex::InvertedIndex(TokenizerOptions tokOpts, BM25Params bm25Params)
    : tokenizer_(tokOpts), scorer_(bm25Params) {}

InvertedIndex::InvertedIndex(InvertedIndex&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.mutex_);
    tokenizer_ = std::move(other.tokenizer_);
    scorer_ = std::move(other.scorer_);
    dictionary_ = std::move(other.dictionary_);
    docLengths_ = std::move(other.docLengths_);
    totalTokens_ = other.totalTokens_;
}

InvertedIndex& InvertedIndex::operator=(InvertedIndex&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lockThis(mutex_, std::defer_lock);
        std::unique_lock<std::shared_mutex> lockOther(other.mutex_, std::defer_lock);
        std::lock(lockThis, lockOther);
        tokenizer_ = std::move(other.tokenizer_);
        scorer_ = std::move(other.scorer_);
        dictionary_ = std::move(other.dictionary_);
        docLengths_ = std::move(other.docLengths_);
        totalTokens_ = other.totalTokens_;
    }
    return *this;
}

void InvertedIndex::indexDocument(uint64_t docId, std::string_view content) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (docLengths_.find(docId) != docLengths_.end()) {
        lock.unlock();
        removeDocument(docId);
        lock.lock();
    }

    auto tokens = tokenizer_.tokenize(content);
    docLengths_[docId] = tokens.size();
    totalTokens_ += tokens.size();

    for (const auto& tok : tokens) {
        dictionary_[tok.term].addOccurrence(docId, tok.position);
    }
}

bool InvertedIndex::removeDocument(uint64_t docId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = docLengths_.find(docId);
    if (it == docLengths_.end()) {
        return false;
    }

    totalTokens_ -= it->second;
    docLengths_.erase(it);

    auto dictIt = dictionary_.begin();
    while (dictIt != dictionary_.end()) {
        auto& pList = dictIt->second;
        PostingList updated;
        for (const auto& p : pList.postings()) {
            if (p.docId != docId) {
                updated.addPosting(p);
            }
        }
        if (updated.docFrequency() == 0) {
            dictIt = dictionary_.erase(dictIt);
        } else {
            dictIt->second = std::move(updated);
            ++dictIt;
        }
    }

    return true;
}

bool InvertedIndex::hasDocument(uint64_t docId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return docLengths_.find(docId) != docLengths_.end();
}

std::vector<SearchResult> InvertedIndex::search(std::string_view query, QueryOptions options) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto queryTokens = tokenizer_.tokenize(query);
    if (queryTokens.empty() || docLengths_.empty()) {
        return {};
    }

    const size_t N = docLengths_.size();
    const double avgdl = averageDocumentLength();

    // Map docId -> accumulator
    struct Accumulator {
        double score{0.0};
        std::vector<std::string> matched;
    };
    std::unordered_map<uint64_t, Accumulator> scores;

    // Remove duplicate terms in query for clean scoring
    std::vector<std::string> uniqueTerms;
    for (const auto& tok : queryTokens) {
        if (std::find(uniqueTerms.begin(), uniqueTerms.end(), tok.term) == uniqueTerms.end()) {
            uniqueTerms.push_back(tok.term);
        }
    }

    for (const auto& term : uniqueTerms) {
        auto it = dictionary_.find(term);
        if (it == dictionary_.end()) {
            continue;
        }

        const auto& pList = it->second;
        double idfVal = scorer_.idf(N, pList.docFrequency());

        for (const auto& posting : pList.postings()) {
            auto docLenIt = docLengths_.find(posting.docId);
            size_t docLen = (docLenIt != docLengths_.end()) ? docLenIt->second : 0;

            double termScore = scorer_.scoreTerm(idfVal, posting.termFrequency, docLen, avgdl);
            auto& acc = scores[posting.docId];
            acc.score += termScore;
            acc.matched.push_back(term);
        }
    }

    std::vector<SearchResult> results;
    results.reserve(scores.size());

    for (auto& [docId, acc] : scores) {
        if (options.requireAllTerms && acc.matched.size() < uniqueTerms.size()) {
            continue;
        }
        if (acc.score < options.minScore) {
            continue;
        }

        SearchResult res;
        res.docId = docId;
        res.score = acc.score;
        res.matchedTerms = std::move(acc.matched);
        results.push_back(std::move(res));
    }

    std::sort(results.begin(), results.end());

    if (results.size() > options.limit) {
        results.resize(options.limit);
    }

    return results;
}

size_t InvertedIndex::totalDocuments() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return docLengths_.size();
}

size_t InvertedIndex::totalTerms() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return dictionary_.size();
}

double InvertedIndex::averageDocumentLength() const noexcept {
    if (docLengths_.empty()) return 0.0;
    return static_cast<double>(totalTokens_) / static_cast<double>(docLengths_.size());
}

size_t InvertedIndex::documentLength(uint64_t docId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = docLengths_.find(docId);
    return (it != docLengths_.end()) ? it->second : 0;
}

std::vector<uint8_t> InvertedIndex::serialize() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<uint8_t> buf;

    // 1. Doc lengths
    utils::VarInt::encode(docLengths_.size(), buf);
    for (const auto& [docId, len] : docLengths_) {
        utils::VarInt::encode(docId, buf);
        utils::VarInt::encode(len, buf);
    }

    // 2. Dictionary
    utils::VarInt::encode(dictionary_.size(), buf);
    for (const auto& [term, pList] : dictionary_) {
        utils::VarInt::encode(term.size(), buf);
        buf.insert(buf.end(), term.begin(), term.end());

        auto pBytes = pList.serialize();
        utils::VarInt::encode(pBytes.size(), buf);
        buf.insert(buf.end(), pBytes.begin(), pBytes.end());
    }

    return buf;
}

std::optional<InvertedIndex> InvertedIndex::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.empty()) return InvertedIndex{};

    size_t offset = 0;
    auto numDocsRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!numDocsRes.valid) return std::nullopt;
    offset += numDocsRes.consumed;

    InvertedIndex index;

    for (uint64_t i = 0; i < numDocsRes.value; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        auto docIdRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!docIdRes.valid) return std::nullopt;
        offset += docIdRes.consumed;

        if (offset >= bytes.size()) return std::nullopt;
        auto lenRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!lenRes.valid) return std::nullopt;
        offset += lenRes.consumed;

        index.docLengths_[docIdRes.value] = static_cast<size_t>(lenRes.value);
        index.totalTokens_ += lenRes.value;
    }

    if (offset >= bytes.size()) return std::nullopt;
    auto numTermsRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!numTermsRes.valid) return std::nullopt;
    offset += numTermsRes.consumed;

    for (uint64_t i = 0; i < numTermsRes.value; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        auto termLenRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!termLenRes.valid) return std::nullopt;
        offset += termLenRes.consumed;

        size_t termLen = static_cast<size_t>(termLenRes.value);
        if (offset + termLen > bytes.size()) return std::nullopt;
        std::string term(reinterpret_cast<const char*>(bytes.data() + offset), termLen);
        offset += termLen;

        if (offset >= bytes.size()) return std::nullopt;
        auto pBytesLenRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!pBytesLenRes.valid) return std::nullopt;
        offset += pBytesLenRes.consumed;

        size_t pBytesLen = static_cast<size_t>(pBytesLenRes.value);
        if (offset + pBytesLen > bytes.size()) return std::nullopt;

        auto pListOpt = PostingList::deserialize(bytes.subspan(offset, pBytesLen));
        if (!pListOpt) return std::nullopt;
        offset += pBytesLen;

        index.dictionary_[term] = std::move(*pListOpt);
    }

    return index;
}

void InvertedIndex::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    dictionary_.clear();
    docLengths_.clear();
    totalTokens_ = 0;
}

} // namespace search
} // namespace nebula
