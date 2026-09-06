#include "nebula/search/Tokenizer.hpp"

#include <cctype>
#include <algorithm>

namespace nebula {
namespace search {

Tokenizer::Tokenizer(TokenizerOptions options) : options_(options) {
    loadDefaultStopwords();
}

void Tokenizer::loadDefaultStopwords() {
    static const char* const kDefaultWords[] = {
        "a", "about", "above", "after", "again", "against", "all", "am", "an", "and",
        "any", "are", "aren't", "as", "at", "be", "because", "been", "before", "being",
        "below", "between", "both", "but", "by", "can't", "cannot", "could", "couldn't",
        "did", "didn't", "do", "does", "doesn't", "doing", "don't", "down", "during",
        "each", "few", "for", "from", "further", "had", "hadn't", "has", "hasn't",
        "have", "haven't", "having", "he", "he'd", "he'll", "he's", "her", "here",
        "here's", "hers", "herself", "him", "himself", "his", "how", "how's", "i",
        "i'd", "i'll", "i'm", "i've", "if", "in", "into", "is", "isn't", "it",
        "it's", "its", "itself", "let's", "me", "more", "most", "mustn't", "my",
        "myself", "no", "nor", "not", "of", "off", "on", "once", "only", "or",
        "other", "ought", "our", "ours", "ourselves", "out", "over", "own", "same",
        "shan't", "she", "she'd", "she'll", "she's", "should", "shouldn't", "so",
        "some", "such", "than", "that", "that's", "the", "their", "theirs", "them",
        "themselves", "then", "there", "there's", "these", "they", "they'd", "they'll",
        "they're", "they've", "this", "those", "through", "to", "too", "under",
        "until", "up", "very", "was", "wasn't", "we", "we'd", "we'll", "we're",
        "we've", "were", "weren't", "what", "what's", "when", "when's", "where",
        "where's", "which", "while", "who", "who's", "whom", "why", "why's", "with",
        "won't", "would", "wouldn't", "you", "you'd", "you'll", "you're", "you've",
        "your", "yours", "yourself", "yourselves"
    };

    for (const char* word : kDefaultWords) {
        stopwords_.emplace(word);
    }
}

bool Tokenizer::isStopword(std::string_view term) const {
    std::string s(term);
    return stopwords_.find(s) != stopwords_.end();
}

void Tokenizer::addStopword(std::string term) {
    if (options_.toLower) {
        std::transform(term.begin(), term.end(), term.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }
    stopwords_.insert(std::move(term));
}

void Tokenizer::removeStopword(const std::string& term) {
    stopwords_.erase(term);
}

std::vector<Token> Tokenizer::tokenize(std::string_view text) const {
    std::vector<Token> tokens;
    if (text.empty()) {
        return tokens;
    }

    uint32_t position = 0;
    size_t i = 0;
    const size_t len = text.size();

    while (i < len) {
        // Skip non-alphanumeric delimiters
        while (i < len && (std::isspace(static_cast<unsigned char>(text[i])) ||
               (options_.removePunctuation && std::ispunct(static_cast<unsigned char>(text[i]))))) {
            i++;
        }

        if (i >= len) break;

        size_t startOffset = i;
        while (i < len && !std::isspace(static_cast<unsigned char>(text[i])) &&
               (!options_.removePunctuation || !std::ispunct(static_cast<unsigned char>(text[i])))) {
            i++;
        }

        size_t termLen = i - startOffset;
        if (termLen >= options_.minTermLength && termLen <= options_.maxTermLength) {
            std::string term(text.substr(startOffset, termLen));
            if (options_.toLower) {
                std::transform(term.begin(), term.end(), term.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            }

            if (!options_.filterStopwords || !isStopword(term)) {
                Token tok;
                tok.term = std::move(term);
                tok.position = position++;
                tok.byteOffset = static_cast<uint32_t>(startOffset);
                tok.byteLength = static_cast<uint32_t>(termLen);
                tokens.push_back(std::move(tok));
            }
        }
    }

    return tokens;
}

std::unordered_map<std::string, uint32_t> Tokenizer::termFrequencies(std::string_view text) const {
    std::unordered_map<std::string, uint32_t> tf;
    auto tokens = tokenize(text);
    for (const auto& tok : tokens) {
        tf[tok.term]++;
    }
    return tf;
}

} // namespace search
} // namespace nebula
