#include "nebula/index/BTree.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>
#include <stack>

namespace nebula {
namespace index {

template<typename Key, typename Value>
void BTree<Key, Value>::insert(const Key& key, const Value& value) {
    auto root = root_;
    if (root->keys.size() == order_ - 1) {
        auto newRoot = std::make_shared<Node>(order_);
        newRoot->isLeaf = false;
        newRoot->children.push_back(root);
        splitChild(newRoot, 0, root);
        root_ = newRoot;
        root = newRoot;
    }
    insertInternal(root_, key, value);
    ++size_;
}

template<typename Key, typename Value>
void BTree<Key, Value>::insertInternal(std::shared_ptr<Node> node,
                                        const Key& key, const Value& value) {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        ++i;
    }

    if (node->isLeaf) {
        node->keys.insert(node->keys.begin() + static_cast<ptrdiff_t>(i), key);
        node->values.insert(node->values.begin() + static_cast<ptrdiff_t>(i), value);
    } else {
        if (node->children[i]->keys.size() == order_ - 1) {
            splitChild(node, i, node->children[i]);
            if (key > node->keys[i]) {
                ++i;
            }
        }
        insertInternal(node->children[i], key, value);
    }
}

template<typename Key, typename Value>
void BTree<Key, Value>::splitChild(std::shared_ptr<Node> parent, size_t index,
                                    std::shared_ptr<Node> child) {
    size_t mid = (order_ - 1) / 2;
    auto newChild = std::make_shared<Node>(order_);
    newChild->isLeaf = child->isLeaf;

    Key midKey = child->keys[mid];
    Value midValue = child->values[mid];

    newChild->keys.assign(child->keys.begin() + static_cast<ptrdiff_t>(mid + 1),
                          child->keys.end());
    newChild->values.assign(child->values.begin() + static_cast<ptrdiff_t>(mid + 1),
                            child->values.end());

    if (!child->isLeaf) {
        newChild->children.assign(child->children.begin() + static_cast<ptrdiff_t>(mid + 1),
                                  child->children.end());
    }

    child->keys.resize(mid);
    child->values.resize(mid);
    if (!child->isLeaf) {
        child->children.resize(mid + 1);
    }

    parent->keys.insert(parent->keys.begin() + static_cast<ptrdiff_t>(index),
                        midKey);
    parent->values.insert(parent->values.begin() + static_cast<ptrdiff_t>(index),
                          midValue);
    parent->children.insert(parent->children.begin() + static_cast<ptrdiff_t>(index + 1),
                            newChild);
}

template<typename Key, typename Value>
std::optional<Value> BTree<Key, Value>::find(const Key& key) const {
    return findInternal(root_, key);
}

template<typename Key, typename Value>
std::optional<Value> BTree<Key, Value>::findInternal(std::shared_ptr<Node> node,
                                                      const Key& key) const {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        ++i;
    }

    if (i < node->keys.size() && key == node->keys[i]) {
        return node->values[i];
    }

    if (node->isLeaf) {
        return std::nullopt;
    }

    return findInternal(node->children[i], key);
}

template<typename Key, typename Value>
bool BTree<Key, Value>::contains(const Key& key) const {
    return find(key).has_value();
}

template<typename Key, typename Value>
bool BTree<Key, Value>::remove(const Key& key) {
    if (!contains(key)) return false;
    removeInternal(root_, key);
    --size_;

    if (root_->keys.empty() && !root_->isLeaf) {
        root_ = root_->children[0];
    }

    return true;
}

template<typename Key, typename Value>
typename BTree<Key, Value>::Iterator BTree<Key, Value>::begin() const {
    return Iterator(root_);
}

template<typename Key, typename Value>
typename BTree<Key, Value>::Iterator BTree<Key, Value>::end() const {
    Iterator it(nullptr);
    return it;
}

template<typename Key, typename Value>
void BTree<Key, Value>::clear() {
    root_ = std::make_shared<Node>(order_);
    size_ = 0;
}

template<typename Key, typename Value>
std::vector<uint8_t> BTree<Key, Value>::serialize() const {
    std::vector<uint8_t> out;
    utils::VarInt::encode(static_cast<uint64_t>(order_), out);
    utils::VarInt::encode(static_cast<uint64_t>(size_), out);
    serializeNode(root_, out);
    return out;
}

template<typename Key, typename Value>
void BTree<Key, Value>::serializeNode(std::shared_ptr<Node> node,
                                       std::vector<uint8_t>& out) const {
    utils::VarInt::encode(static_cast<uint64_t>(node->keys.size()), out);
    utils::VarInt::encode(node->isLeaf ? 1ULL : 0ULL, out);

    for (size_t i = 0; i < node->keys.size(); ++i) {
        utils::VarInt::encode(node->keys[i], out);
        utils::VarInt::encode(node->values[i].entryId, out);
        utils::VarInt::encode(node->values[i].offset, out);
        utils::VarInt::encode(node->values[i].size, out);
        out.insert(out.end(), node->values[i].checksum.begin(),
                   node->values[i].checksum.end());
    }

    if (!node->isLeaf) {
        for (auto& child : node->children) {
            serializeNode(child, out);
        }
    }
}

template<typename Key, typename Value>
std::error_code BTree<Key, Value>::deserialize(std::span<const uint8_t> data) {
    size_t offset = 0;

    auto orderResult = utils::VarInt::decode(data);
    if (!orderResult.valid) return make_error_code(ErrorCode::CorruptIndex);
    order_ = static_cast<size_t>(orderResult.value);
    offset += orderResult.consumed;

    auto sizeResult = utils::VarInt::decode(data.subspan(offset));
    if (!sizeResult.valid) return make_error_code(ErrorCode::CorruptIndex);
    size_ = static_cast<size_t>(sizeResult.value);
    offset += sizeResult.consumed;

    auto remaining = data.subspan(offset);
    root_ = deserializeNode(remaining);

    return std::error_code();
}



template<typename Key, typename Value>
bool BTree<Key, Value>::removeInternal(std::shared_ptr<Node> node, const Key& key) {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        ++i;
    }

    if (i < node->keys.size() && key == node->keys[i]) {
        if (node->isLeaf) {
            node->keys.erase(node->keys.begin() + static_cast<ptrdiff_t>(i));
            node->values.erase(node->values.begin() + static_cast<ptrdiff_t>(i));
        } else {
            auto pred = getPredecessor(node, i);
            node->keys[i] = pred;
            removeInternal(node->children[i], pred);
        }
        return true;
    }

    if (node->isLeaf) {
        return false;
    }

    fillChild(node, i);

    auto& child = node->children[i];
    if (i < node->keys.size() && key > node->keys[i]) {
        ++i;
    }

    return removeInternal(node->children[i], key);
}

template<typename Key, typename Value>
Key BTree<Key, Value>::getPredecessor(std::shared_ptr<Node> node, size_t index) {
    auto current = node->children[index];
    while (!current->isLeaf) {
        current = current->children.back();
    }
    return current->keys.back();
}

template<typename Key, typename Value>
void BTree<Key, Value>::mergeChildren(std::shared_ptr<Node> node, size_t index) {
    auto child = node->children[index];
    auto sibling = node->children[index + 1];

    child->keys.push_back(node->keys[index]);
    child->values.push_back(node->values[index]);

    child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
    child->values.insert(child->values.end(), sibling->values.begin(), sibling->values.end());

    if (!child->isLeaf) {
        child->children.insert(child->children.end(), sibling->children.begin(), sibling->children.end());
    }

    node->keys.erase(node->keys.begin() + static_cast<ptrdiff_t>(index));
    node->values.erase(node->values.begin() + static_cast<ptrdiff_t>(index));
    node->children.erase(node->children.begin() + static_cast<ptrdiff_t>(index + 1));
}

template<typename Key, typename Value>
void BTree<Key, Value>::borrowFromPrev(std::shared_ptr<Node> node, size_t index) {
    auto child = node->children[index];
    auto sibling = node->children[index - 1];

    child->keys.insert(child->keys.begin(), node->keys[index - 1]);
    child->values.insert(child->values.begin(), node->values[index - 1]);

    if (!child->isLeaf) {
        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
    }

    node->keys[index - 1] = sibling->keys.back();
    node->values[index - 1] = sibling->values.back();
    sibling->keys.pop_back();
    sibling->values.pop_back();
}

template<typename Key, typename Value>
void BTree<Key, Value>::borrowFromNext(std::shared_ptr<Node> node, size_t index) {
    auto child = node->children[index];
    auto sibling = node->children[index + 1];

    child->keys.push_back(node->keys[index]);
    child->values.push_back(node->values[index]);

    if (!child->isLeaf) {
        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }

    node->keys[index] = sibling->keys.front();
    node->values[index] = sibling->values.front();
    sibling->keys.erase(sibling->keys.begin());
    sibling->values.erase(sibling->values.begin());
}

template<typename Key, typename Value>
void BTree<Key, Value>::fillChild(std::shared_ptr<Node> node, size_t index) {
    size_t minKeys = (order_ - 1) / 2;

    if (index > 0 && node->children[index - 1]->keys.size() > minKeys) {
        borrowFromPrev(node, index);
    } else if (index < node->children.size() - 1 && node->children[index + 1]->keys.size() > minKeys) {
        borrowFromNext(node, index);
    } else {
        if (index == node->children.size() - 1) {
            mergeChildren(node, index - 1);
        } else {
            mergeChildren(node, index);
        }
    }
}

template<typename Key, typename Value>
std::shared_ptr<typename BTree<Key, Value>::Node> BTree<Key, Value>::deserializeNode(
    std::span<const uint8_t>& data) {
    auto keyCountResult = utils::VarInt::decode(data);
    if (!keyCountResult.valid) return nullptr;

    size_t consumed = keyCountResult.consumed;
    size_t numKeys = static_cast<size_t>(keyCountResult.value);

    auto isLeafResult = utils::VarInt::decode(data.subspan(consumed));
    if (!isLeafResult.valid) return nullptr;
    consumed += isLeafResult.consumed;
    bool isLeaf = isLeafResult.value != 0;

    auto node = std::make_shared<Node>(order_);
    node->isLeaf = isLeaf;

    for (size_t i = 0; i < numKeys; ++i) {
        auto keyResult = utils::VarInt::decode(data.subspan(consumed));
        if (!keyResult.valid) return nullptr;
        consumed += keyResult.consumed;
        node->keys.push_back(static_cast<Key>(keyResult.value));

        if constexpr (std::is_same_v<Value, IndexEntry>) {
            auto entryIdResult = utils::VarInt::decode(data.subspan(consumed));
            if (!entryIdResult.valid) return nullptr;
            consumed += entryIdResult.consumed;

            auto offsetResult = utils::VarInt::decode(data.subspan(consumed));
            if (!offsetResult.valid) return nullptr;
            consumed += offsetResult.consumed;

            auto sizeResult = utils::VarInt::decode(data.subspan(consumed));
            if (!sizeResult.valid) return nullptr;
            consumed += sizeResult.consumed;

            IndexEntry entry;
            entry.entryId = static_cast<uint64_t>(entryIdResult.value);
            entry.offset = static_cast<uint64_t>(offsetResult.value);
            entry.size = static_cast<uint64_t>(sizeResult.value);
            if (consumed + 32 <= data.size()) {
                std::memcpy(entry.checksum.data(), &data[consumed], 32);
                consumed += 32;
            }
            node->values.push_back(entry);
        }
    }

    if (!isLeaf) {
        for (size_t i = 0; i <= numKeys; ++i) {
            data = data.subspan(consumed);
            consumed = 0;
            auto child = deserializeNode(data);
            if (!child) return nullptr;
            node->children.push_back(child);
        }
    }

    data = data.subspan(consumed);
    return node;
}

} // namespace index
} // namespace nebula

template class nebula::index::BTree<uint64_t, nebula::IndexEntry>;
