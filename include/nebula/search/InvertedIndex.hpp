#pragma once

#include "nebula/search/Tokenizer.hpp"
#include "nebula/search/PostingList.hpp"
#include "nebula/search/BM25Scorer.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace nebula {
namespace search {

struct SearchResult {
    uint64_t docId{0};
    double score{0.0};
    std::vector<std::string> matchedTerms;

    bool operator<(const SearchResult& other) const noexcept {
        return score > other.score; // Descending order
    }
};

struct QueryOptions {
    size_t limit{10};
    double minScore{0.0};
    bool requireAllTerms{false};
};

class InvertedIndex {
public:
    explicit InvertedIndex(TokenizerOptions tokOpts = {}, BM25Params bm25Params = {});
    ~InvertedIndex() = default;

    InvertedIndex(InvertedIndex&& other) noexcept;
    InvertedIndex& operator=(InvertedIndex&& other) noexcept;
    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;

    void indexDocument(uint64_t docId, std::string_view content);
    bool removeDocument(uint64_t docId);
    bool hasDocument(uint64_t docId) const;

    std::vector<SearchResult> search(std::string_view query, QueryOptions options = {}) const;

    [[nodiscard]] size_t totalDocuments() const noexcept;
    [[nodiscard]] size_t totalTerms() const noexcept;
    [[nodiscard]] double averageDocumentLength() const noexcept;
    [[nodiscard]] size_t documentLength(uint64_t docId) const;

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<InvertedIndex> deserialize(std::span<const uint8_t> bytes);

    void clear() noexcept;

private:
    Tokenizer tokenizer_;
    BM25Scorer scorer_;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, PostingList> dictionary_;
    std::unordered_map<uint64_t, size_t> docLengths_;
    uint64_t totalTokens_{0};
};

} // namespace search
} // namespace nebula
