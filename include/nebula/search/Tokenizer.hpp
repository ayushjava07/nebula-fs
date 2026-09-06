#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace nebula {
namespace search {

struct Token {
    std::string term;
    uint32_t position{0};
    uint32_t byteOffset{0};
    uint32_t byteLength{0};

    bool operator==(const Token& other) const {
        return term == other.term && position == other.position &&
               byteOffset == other.byteOffset && byteLength == other.byteLength;
    }
};

struct TokenizerOptions {
    bool toLower{true};
    bool removePunctuation{true};
    bool filterStopwords{true};
    size_t minTermLength{2};
    size_t maxTermLength{64};
};

class Tokenizer {
public:
    explicit Tokenizer(TokenizerOptions options = {});

    std::vector<Token> tokenize(std::string_view text) const;
    std::unordered_map<std::string, uint32_t> termFrequencies(std::string_view text) const;

    [[nodiscard]] bool isStopword(std::string_view term) const;
    void addStopword(std::string term);
    void removeStopword(const std::string& term);

    [[nodiscard]] const TokenizerOptions& options() const noexcept { return options_; }
    void setOptions(const TokenizerOptions& options) { options_ = options; }

private:
    TokenizerOptions options_;
    std::unordered_set<std::string> stopwords_;

    void loadDefaultStopwords();
};

} // namespace search
} // namespace nebula
