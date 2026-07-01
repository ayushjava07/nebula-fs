#include "nebula/filesystem/BTree.hpp"
#include <stack>
#include <functional>

namespace nebula {
namespace filesystem {

template<typename Key, typename Value>
void BTree<Key, Value>::insert(const Key& key, const Value& value) {
    if (root_->keys.size() == order_ - 1) {
        auto* newRoot = new Node(order_);
        newRoot->isLeaf = false;
        newRoot->children.push_back(root_);
        root_->parent = newRoot;
        splitChild(newRoot, 0, root_);
        root_ = newRoot;
    }
    insertInternal(root_, key, value);
    ++size_;
}

template<typename Key, typename Value>
void BTree<Key, Value>::insertInternal(Node* node, const Key& key, const Value& value) {
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
void BTree<Key, Value>::splitChild(Node* parent, size_t index, Node* child) {
    size_t mid = (order_ - 1) / 2;
    auto* newChild = new Node(order_);
    newChild->isLeaf = child->isLeaf;
    newChild->parent = parent;

    Key midKey = child->keys[mid];
    Value midValue = child->values[mid];

    newChild->keys.assign(child->keys.begin() + static_cast<ptrdiff_t>(mid + 1),
                          child->keys.end());
    newChild->values.assign(child->values.begin() + static_cast<ptrdiff_t>(mid + 1),
                            child->values.end());

    if (!child->isLeaf) {
        newChild->children.assign(child->children.begin() + static_cast<ptrdiff_t>(mid + 1),
                                  child->children.end());
        for (auto* c : newChild->children) {
            c->parent = newChild;
        }
    }

    child->keys.resize(mid);
    child->values.resize(mid);
    if (!child->isLeaf) {
        child->children.resize(mid + 1);
    }

    parent->keys.insert(parent->keys.begin() + static_cast<ptrdiff_t>(index), midKey);
    parent->values.insert(parent->values.begin() + static_cast<ptrdiff_t>(index), midValue);
    parent->children.insert(parent->children.begin() + static_cast<ptrdiff_t>(index + 1), newChild);
}

template<typename Key, typename Value>
std::optional<Value> BTree<Key, Value>::find(const Key& key) const {
    return findInternal(root_, key);
}

template<typename Key, typename Value>
std::optional<Value> BTree<Key, Value>::findInternal(Node* node, const Key& key) const {
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
    if (empty()) return false;
    bool removed = removeInternal(root_, key);
    if (removed) {
        --size_;
        if (root_->keys.empty() && !root_->isLeaf) {
            Node* oldRoot = root_;
            root_ = root_->children[0];
            root_->parent = nullptr;
            delete oldRoot;
        }
    }
    return removed;
}

template<typename Key, typename Value>
bool BTree<Key, Value>::removeInternal(Node* node, const Key& key) {
    size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        ++i;
    }

    if (i < node->keys.size() && key == node->keys[i]) {
        if (node->isLeaf) {
            node->keys.erase(node->keys.begin() + static_cast<ptrdiff_t>(i));
            node->values.erase(node->values.begin() + static_cast<ptrdiff_t>(i));
            return true;
        }
        Key pred = getPredecessor(node, i);
        node->keys[i] = pred;
        node->values[i] = findInternal(node->children[i], pred).value();
        return removeInternal(node->children[i], pred);
    }

    if (node->isLeaf) return false;

    fillChild(node, i);
    return removeInternal(node->children[i], key);
}

template<typename Key, typename Value>
Key BTree<Key, Value>::getPredecessor(Node* node, size_t index) {
    Node* cur = node->children[index];
    while (!cur->isLeaf) {
        cur = cur->children.back();
    }
    return cur->keys.back();
}

template<typename Key, typename Value>
void BTree<Key, Value>::mergeChildren(Node* parent, size_t index) {
    Node* left = parent->children[index];
    Node* right = parent->children[index + 1];

    left->keys.push_back(parent->keys[index]);
    left->values.push_back(parent->values[index]);

    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->values.insert(left->values.end(), right->values.begin(), right->values.end());

    if (!left->isLeaf) {
        left->children.insert(left->children.end(), right->children.begin(), right->children.end());
        for (auto* c : right->children) {
            c->parent = left;
        }
    }

    parent->keys.erase(parent->keys.begin() + static_cast<ptrdiff_t>(index));
    parent->values.erase(parent->values.begin() + static_cast<ptrdiff_t>(index));
    parent->children.erase(parent->children.begin() + static_cast<ptrdiff_t>(index + 1));

    delete right;
}

template<typename Key, typename Value>
void BTree<Key, Value>::borrowFromPrev(Node* node, size_t index) {
    Node* child = node->children[index];
    Node* sibling = node->children[index - 1];

    child->keys.insert(child->keys.begin(), node->keys[index - 1]);
    child->values.insert(child->values.begin(), node->values[index - 1]);

    if (!child->isLeaf) {
        child->children.insert(child->children.begin(), sibling->children.back());
        child->children.front()->parent = child;
        sibling->children.pop_back();
    }

    node->keys[index - 1] = sibling->keys.back();
    node->values[index - 1] = sibling->values.back();

    sibling->keys.pop_back();
    sibling->values.pop_back();
}

template<typename Key, typename Value>
void BTree<Key, Value>::borrowFromNext(Node* node, size_t index) {
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];

    child->keys.push_back(node->keys[index]);
    child->values.push_back(node->values[index]);

    if (!child->isLeaf) {
        child->children.push_back(sibling->children.front());
        child->children.back()->parent = child;
        sibling->children.erase(sibling->children.begin());
    }

    node->keys[index] = sibling->keys.front();
    node->values[index] = sibling->values.front();

    sibling->keys.erase(sibling->keys.begin());
    sibling->values.erase(sibling->values.begin());
}

template<typename Key, typename Value>
void BTree<Key, Value>::fillChild(Node* node, size_t index) {
    if (index > 0 && node->children[index - 1]->keys.size() >= (order_ - 1) / 2) {
        borrowFromPrev(node, index);
    } else if (index < node->children.size() - 1 &&
               node->children[index + 1]->keys.size() >= (order_ - 1) / 2) {
        borrowFromNext(node, index);
    } else {
        if (index < node->children.size() - 1) {
            mergeChildren(node, index);
        } else {
            mergeChildren(node, index - 1);
        }
    }
}

template<typename Key, typename Value>
typename BTree<Key, Value>::Iterator BTree<Key, Value>::begin() const {
    return Iterator(root_);
}

template<typename Key, typename Value>
typename BTree<Key, Value>::Iterator BTree<Key, Value>::end() const {
    return Iterator(nullptr);
}

template<typename Key, typename Value>
void BTree<Key, Value>::clear(Node* node) {
    if (!node) return;
    if (!node->isLeaf) {
        for (auto* child : node->children) {
            clear(child);
        }
    }
    delete node;
}

// BUG #13: traverseAndModify modifies the tree structure during iteration.
// This invalidates iterators and can cause use-after-free or corruption.
template<typename Key, typename Value>
void BTree<Key, Value>::traverseAndModify(std::function<void(Key&, Value&)> modifier) {
    // BUG: uses raw Node* pointers and modifies keys/values while iterating.
    // If the modifier inserts or removes elements, the tree structure changes
    // under the iterator causing invalidation.
    std::stack<Node*> nodeStack;
    nodeStack.push(root_);

    while (!nodeStack.empty()) {
        Node* node = nodeStack.top();
        nodeStack.pop();

        if (!node) continue;

        // First push children in reverse order so they're processed left-to-right.
        if (!node->isLeaf) {
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
                nodeStack.push(*it);
            }
        }

        // BUG: modifying keys/values while iterating. If the modifier calls
        // insert() or remove(), the current node may be deleted or restructured,
        // making 'node' a dangling pointer for the next iteration.
        for (size_t i = 0; i < node->keys.size(); ++i) {
            modifier(node->keys[i], node->values[i]);
            // BUG: if insert() was called inside modifier, node pointers
            // may be invalidated (reallocation of children vector, or
            // node was split). The loop index 'i' may no longer be valid.
        }
    }
}

// Explicit instantiations for common types
template class BTree<std::string, EntryID>;

} // namespace filesystem
} // namespace nebula
