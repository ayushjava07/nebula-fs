#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

namespace nebula {
namespace filesystem {

/// A B-Tree implementation for filesystem directory indexing.
///
/// Maps filenames to entry IDs with O(log n) operations.
template<typename Key = std::string, typename Value = EntryID>
class BTree {
public:
    struct Node {
        std::vector<Key> keys;
        std::vector<Value> values;
        std::vector<Node*> children;
        bool isLeaf = true;
        Node* parent = nullptr;

        explicit Node(size_t order)
            : keys(), values(), children(), isLeaf(true), parent(nullptr) {
            keys.reserve(order - 1);
            values.reserve(order - 1);
            children.reserve(order);
        }
    };

    class Iterator {
    public:
        using value_type = std::pair<Key, Value>;

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

        value_type operator*() const {
            auto& [node, pos] = stack_.back();
            size_t idx = node->isLeaf ? pos : (pos / 2);
            return {node->keys[idx], node->values[idx]};
        }

    private:
        friend class BTree;
        using StackFrame = std::pair<Node*, size_t>;
        std::vector<StackFrame> stack_;

        explicit Iterator(Node* root) {
            if (root) {
                stack_.emplace_back(root, 0);
                descend();
            }
        }

        void descend() {
            while (true) {
                auto& [node, pos] = stack_.back();
                if (node->isLeaf) break;
                if (pos % 2 == 0 && pos / 2 < node->children.size()) {
                    stack_.emplace_back(node->children[pos / 2], 0);
                } else {
                    break;
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
                    stack_.emplace_back(node->children[pos / 2], 0);
                    descend();
                }
                break;
            }
        }
    };

    explicit BTree(size_t order = kBTreeDefaultOrder)
        : order_(order), root_(new Node(order)), size_(0) {}

    ~BTree() noexcept {
        clear(root_);
    }

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;
    BTree(BTree&& other) noexcept
        : order_(other.order_), root_(other.root_), size_(other.size_) {
        other.root_ = nullptr;
        other.size_ = 0;
    }
    BTree& operator=(BTree&& other) noexcept {
        if (this != &other) {
            clear(root_);
            order_ = other.order_;
            root_ = other.root_;
            size_ = other.size_;
            other.root_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void insert(const Key& key, const Value& value);
    std::optional<Value> find(const Key& key) const;
    bool contains(const Key& key) const;
    bool remove(const Key& key);

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    Iterator begin() const;
    Iterator end() const;

    /// BUG #13: Method that modifies the tree during iteration.
    /// Causes iterator invalidation.
    void traverseAndModify(std::function<void(Key&, Value&)> modifier);

private:
    size_t order_;
    Node* root_;
    size_t size_;

    void clear(Node* node);
    void insertInternal(Node* node, const Key& key, const Value& value);
    void splitChild(Node* parent, size_t index, Node* child);
    std::optional<Value> findInternal(Node* node, const Key& key) const;
    bool removeInternal(Node* node, const Key& key);
    Key getPredecessor(Node* node, size_t index);
    void mergeChildren(Node* node, size_t index);
    void borrowFromPrev(Node* node, size_t index);
    void borrowFromNext(Node* node, size_t index);
    void fillChild(Node* node, size_t index);
};

} // namespace filesystem
} // namespace nebula
