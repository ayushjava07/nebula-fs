#include <gtest/gtest.h>
#include "nebula/search/Tokenizer.hpp"
#include "nebula/search/PostingList.hpp"
#include "nebula/search/BM25Scorer.hpp"
#include "nebula/search/InvertedIndex.hpp"

using namespace nebula::search;

// --- Tokenizer Tests ---

TEST(TokenizerTest, BasicTokenizationAndStopwords) {
    Tokenizer tokenizer;
    std::string text = "The quick brown fox jumps over the lazy dog!";
    auto tokens = tokenizer.tokenize(text);

    // "the" is filtered out by default
    EXPECT_FALSE(tokens.empty());
    for (const auto& tok : tokens) {
        EXPECT_NE(tok.term, "the");
        EXPECT_FALSE(tok.term.empty());
    }

    // Verify "quick", "brown", "fox", "jumps", "lazy", "dog" are present
    std::vector<std::string> terms;
    for (const auto& tok : tokens) {
        terms.push_back(tok.term);
    }
    EXPECT_NE(std::find(terms.begin(), terms.end(), "quick"), terms.end());
    EXPECT_NE(std::find(terms.begin(), terms.end(), "fox"), terms.end());
    EXPECT_NE(std::find(terms.begin(), terms.end(), "dog"), terms.end());
}

TEST(TokenizerTest, PunctuationAndCaseNormalization) {
    TokenizerOptions opts;
    opts.filterStopwords = false;
    Tokenizer tokenizer(opts);

    auto tokens = tokenizer.tokenize("Nebula-FS: High-Performance Object Store!!!");
    ASSERT_GE(tokens.size(), 4);
    EXPECT_EQ(tokens[0].term, "nebula");
    EXPECT_EQ(tokens[1].term, "fs");
    EXPECT_EQ(tokens[2].term, "high");
    EXPECT_EQ(tokens[3].term, "performance");
}

// --- PostingList Tests ---

TEST(PostingListTest, AddAndFindPostings) {
    PostingList pList;
    pList.addOccurrence(10, 0);
    pList.addOccurrence(10, 5);
    pList.addOccurrence(20, 2);

    EXPECT_EQ(pList.docFrequency(), 2);
    EXPECT_EQ(pList.totalTermFrequency(), 3);

    auto doc10 = pList.findDoc(10);
    ASSERT_TRUE(doc10.has_value());
    EXPECT_EQ(doc10->termFrequency, 2);
    EXPECT_EQ(doc10->positions, (std::vector<uint32_t>{0, 5}));

    auto doc20 = pList.findDoc(20);
    ASSERT_TRUE(doc20.has_value());
    EXPECT_EQ(doc20->termFrequency, 1);
}

TEST(PostingListTest, SerializeDeserializeRoundTrip) {
    PostingList original;
    original.addOccurrence(100, 1);
    original.addOccurrence(100, 4);
    original.addOccurrence(250, 10);
    original.addOccurrence(500, 3);

    auto bytes = original.serialize();
    EXPECT_FALSE(bytes.empty());

    auto restored = PostingList::deserialize(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->docFrequency(), original.docFrequency());
    EXPECT_EQ(restored->totalTermFrequency(), original.totalTermFrequency());
    EXPECT_EQ(restored->postings(), original.postings());
}

// --- BM25Scorer Tests ---

TEST(BM25ScorerTest, IdfAndTermScoringProperties) {
    BM25Scorer scorer;

    // Rare term (docFreq=1 out of 100) vs common term (docFreq=80 out of 100)
    double rareIdf = scorer.idf(100, 1);
    double commonIdf = scorer.idf(100, 80);
    EXPECT_GT(rareIdf, commonIdf);

    // Higher term frequency in same document yields higher score
    double scoreTf1 = scorer.scoreTerm(rareIdf, 1, 100, 100.0);
    double scoreTf5 = scorer.scoreTerm(rareIdf, 5, 100, 100.0);
    EXPECT_GT(scoreTf5, scoreTf1);

    // Shorter document with same term frequency scores higher (length normalization)
    double scoreShortDoc = scorer.scoreTerm(rareIdf, 2, 50, 100.0);
    double scoreLongDoc  = scorer.scoreTerm(rareIdf, 2, 200, 100.0);
    EXPECT_GT(scoreShortDoc, scoreLongDoc);
}

// --- InvertedIndex Tests ---

TEST(InvertedIndexTest, IndexSearchAndRanking) {
    InvertedIndex index;

    index.indexDocument(1, "distributed file storage and archive systems");
    index.indexDocument(2, "embedded database indexing with btree and hashtable");
    index.indexDocument(3, "high throughput distributed archive indexing database");

    EXPECT_EQ(index.totalDocuments(), 3);
    EXPECT_GT(index.totalTerms(), 5);

    // Query for "distributed archive" -> doc 3 and doc 1 match
    auto results = index.search("distributed archive");
    ASSERT_GE(results.size(), 2);
    // Both doc 1 and doc 3 match, doc 3 has both terms plus database and indexing
    EXPECT_EQ(results[0].matchedTerms.size(), 2);

    // Require all terms
    QueryOptions requireAll;
    requireAll.requireAllTerms = true;
    auto andResults = index.search("database indexing", requireAll);
    ASSERT_EQ(andResults.size(), 2);
    for (const auto& r : andResults) {
        EXPECT_TRUE(r.docId == 2 || r.docId == 3);
    }

    // Remove doc 2
    EXPECT_TRUE(index.removeDocument(2));
    EXPECT_EQ(index.totalDocuments(), 2);
    auto afterRemove = index.search("btree");
    EXPECT_TRUE(afterRemove.empty());
}

TEST(InvertedIndexTest, SerializationRoundTrip) {
    InvertedIndex index;
    index.indexDocument(10, "super fast binary storage protocol");
    index.indexDocument(20, "secure encrypted journal log entries");

    auto bytes = index.serialize();
    EXPECT_FALSE(bytes.empty());

    auto restored = InvertedIndex::deserialize(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->totalDocuments(), 2);

    auto res = restored->search("protocol");
    ASSERT_FALSE(res.empty());
    EXPECT_EQ(res[0].docId, 10);
}
