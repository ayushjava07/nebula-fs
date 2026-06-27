#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include <algorithm>
#include <system_error>

namespace nebula {
namespace index {

/// B-Tree index for efficient lookup by EntryID.
///
/// Provides O(log n) insertion, lookup, and deletion.
/// The tree order (maximum children per node) is configurable.
template<typename Key = EntryID, typename Value = IndexEntry>
class BTree {
public:
    /// Node in the B-tree
    struct Node {
        std::vector<Key> keys;
        std::vector<Value> values;
        std::vector<std::shared_ptr<Node>> children;
        bool isLeaf = true;

        explicit Node(size_t order) {
            keys.reserve(order - 1);
            values.reserve(order - 1);
            children.reserve(order);
        }
    };

    /// Iterator for traversing entries in order
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<Key, Value>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        Iterator& operator++() {
            advance();
            return *this;
        }

        bool operator==(const Iterator& other) const {
            if (stack_.empty() && other.stack_.empty()) return true;
            if (stack_.size() != other.stack_.size()) return false;
            for (size_t i = 0; i < stack_.size(); ++i) {
                if (stack_[i].first != other.stack_[i].first) return false;
                if (stack_[i].second != other.stack_[i].second) return false;
            }
            return true;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

        reference operator*() const {
            auto& [node, pos] = stack_.back();
            size_t keyIdx = node->isLeaf ? pos : (pos / 2);
            current_ = std::pair<Key, Value>(node->keys[keyIdx], node->values[keyIdx]);
            return current_;
        }

        pointer operator->() const {
            operator*();
            return &current_;
        }

    private:
        friend class BTree;
        using StackFrame = std::pair<std::shared_ptr<Node>, size_t>;
        std::vector<StackFrame> stack_;
        mutable value_type current_;

        explicit Iterator(std::shared_ptr<Node> root) {
            if (root) {
                stack_.emplace_back(root, 0);
                while (true) {
                    auto& [node, pos] = stack_.back();
                    if (node->isLeaf) break;
                    if (pos % 2 == 0 && pos / 2 < node->children.size()) {
                        auto child = node->children[pos / 2];
                        stack_.emplace_back(child, 0);
                    } else {
                        break;
                    }
                }
            }
        }

        void advance() {
            while (!stack_.empty()) {
                auto& [node, pos] = stack_.back();
                ++pos;
                size_t maxPos = node->isLeaf ? node->keys.size()
                    : (node->children.size() + node->keys.size());
                if (pos >= maxPos) {
                    stack_.pop_back();
                    continue;
                }
                if (node->isLeaf) break;
                if (pos % 2 == 0 && pos / 2 < node->children.size()) {
                    auto child = node->children[pos / 2];
                    stack_.emplace_back(child, 0);
                    while (true) {
                        auto& [n, p] = stack_.back();
                        if (n->isLeaf) break;
                        if (p % 2 == 0 && p / 2 < n->children.size()) {
                            auto c = n->children[p / 2];
                            stack_.emplace_back(c, 0);
                        } else {
                            break;
                        }
                    }
                }
                break;
            }
        }
    };

    /// Construct a B-tree with the given order
    explicit BTree(size_t order = kBTreeDefaultOrder)
        : order_(order), root_(std::make_shared<Node>(order)) {}

    /// Insert a key-value pair
    void insert(const Key& key, const Value& value);

    /// Find a value by key
    [[nodiscard]] std::optional<Value> find(const Key& key) const;

    /// Check if a key exists
    [[nodiscard]] bool contains(const Key& key) const;

    /// Remove a key-value pair
    bool remove(const Key& key);

    /// Get the number of entries
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// Clear the tree
    void clear();

    /// Begin iterator (in-order)
    [[nodiscard]] Iterator begin() const;

    /// End iterator
    [[nodiscard]] Iterator end() const;

    /// Serialize to vector of bytes
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from bytes
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

private:
    size_t order_;
    size_t size_ = 0;
    std::shared_ptr<Node> root_;

    void insertInternal(std::shared_ptr<Node> node, const Key& key, const Value& value);
    void splitChild(std::shared_ptr<Node> parent, size_t index, std::shared_ptr<Node> child);
    std::optional<Value> findInternal(std::shared_ptr<Node> node, const Key& key) const;
    bool removeInternal(std::shared_ptr<Node> node, const Key& key);
    Key getPredecessor(std::shared_ptr<Node> node, size_t index);
    void mergeChildren(std::shared_ptr<Node> node, size_t index);
    void borrowFromPrev(std::shared_ptr<Node> node, size_t index);
    void borrowFromNext(std::shared_ptr<Node> node, size_t index);
    void fillChild(std::shared_ptr<Node> node, size_t index);

    void serializeNode(std::shared_ptr<Node> node, std::vector<uint8_t>& out) const;
    std::shared_ptr<Node> deserializeNode(std::span<const uint8_t>& data);
};

} // namespace index
} // namespace nebula
