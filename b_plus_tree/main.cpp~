#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename Key, typename Value, std::size_t Order>
class BPlusTree {
  static_assert(Order >= 3, "B+Tree order must be at least 3");
  struct Node {
    explicit Node(bool is_leaf) : leaf(is_leaf), parent(nullptr), next(nullptr), prev(nullptr) {}
    bool leaf;
    std::vector<Key> keys;
    std::vector<Value> values; // Valid only when the node is leaf
    std::vector<std::unique_ptr<Node>> children; // Valid when the node is internal
    Node* parent;
    Node* next;
    Node* prev;
  };

public:
  using key_type = Key;
  using mapped_type = Value;
  BPlusTree() : root_(std::make_unique<Node>(true)) {}
  bool empty() const noexcept { return root_->leaf && root_->keys.empty(); }
  void clear() { root_ = std::make_unique<Node>(true); }
  void insert(const Key& key, const Value& value) {
    Node* leaf = findLeaf(key);
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    const std::size_t index = static_cast<std::size_t>(std::distance(leaf->keys.begin(), it));

    if (it != leaf->keys.end() && *it == key) {
      leaf->values[index] = value;
      return;
    }

    leaf->keys.insert(it, key);
    leaf->values.insert(leaf->values.begin() + index, value);

    if (leaf->keys.size() > maxKeys()) {
      splitLeaf(leaf);
    } else if (leaf->parent && index == 0) {
      // Keep parent separators in sync when this leaf now owns a new minimal key.
      updateParentKeyForChild(leaf);
    }
  }
  std::optional<Value> find(const Key& key) const {
    const Node* leaf = findLeaf(key);
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it != leaf->keys.end() && *it == key) {
      return leaf->values[static_cast<std::size_t>(std::distance(leaf->keys.begin(), it))];
    }
    return std::nullopt;
  }
  std::vector<Value> range(const Key& start, const Key& end_inclusive) const {
    if (start > end_inclusive) { return {}; }
    const Node* leaf = findLeaf(start);
    std::vector<Value> result;
    while (leaf) {
      for (std::size_t i = 0; i < leaf->keys.size(); ++i) {
        if (leaf->keys[i] < start) {
          continue;
        }
        if (leaf->keys[i] > end_inclusive) {
          return result;
        }
        result.push_back(leaf->values[i]);
      }
      leaf = leaf->next;
    }
    return result;
  }
  bool erase(const Key& key) {
    Node* leaf = findLeaf(key);
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it == leaf->keys.end() || *it != key) {
      return false;
    }
    std::size_t index = static_cast<std::size_t>(std::distance(leaf->keys.begin(), it));
    leaf->keys.erase(it);
    leaf->values.erase(leaf->values.begin() + static_cast<std::ptrdiff_t>(index));

    if (leaf == root_.get()) {
      return true;  // Root can stay empty when leaf
    }

    if (!leaf->keys.empty() && index == 0) {
      updateParentKeyForChild(leaf);
    }

    if (leaf->keys.size() < minKeysLeaf()) {
      rebalanceAfterDeletion(leaf);
    }
    return true;
  }
private:
  static constexpr std::size_t maxChildren() { return Order; }
    static constexpr std::size_t maxKeys() { return Order - 1; }
    static constexpr std::size_t minChildren() { return (Order + 1) / 2; }
    static constexpr std::size_t minKeysInternal() { return minChildren() - 1; }
    static constexpr std::size_t minKeysLeaf() { return minChildren() - 1; }

    Node* findLeaf(const Key& key) const {
        Node* node = root_.get();
        while (!node->leaf) {
            auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
            std::size_t index = static_cast<std::size_t>(std::distance(node->keys.begin(), it));
            node = node->children[index].get();
        }
        return node;
    }

    void splitLeaf(Node* leaf) {
        auto new_leaf = std::make_unique<Node>(true);
        std::size_t mid = leaf->keys.size() / 2;
        new_leaf->keys.assign(leaf->keys.begin() + static_cast<std::ptrdiff_t>(mid), leaf->keys.end());
        new_leaf->values.assign(leaf->values.begin() + static_cast<std::ptrdiff_t>(mid), leaf->values.end());

        leaf->keys.resize(mid);
        leaf->values.resize(mid);

        new_leaf->next = leaf->next;
        if (new_leaf->next) {
            new_leaf->next->prev = new_leaf.get();
        }
        leaf->next = new_leaf.get();
        new_leaf->prev = leaf;

        insertIntoParent(leaf, new_leaf->keys.front(), std::move(new_leaf));
    }

    void splitInternal(Node* node) {
        auto new_node = std::make_unique<Node>(false);
        std::size_t mid = node->keys.size() / 2;
        Key up_key = node->keys[mid];

        new_node->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), node->keys.end());
        node->keys.resize(mid);

        for (std::size_t i = mid + 1; i < node->children.size(); ++i) {
            std::unique_ptr<Node> child = std::move(node->children[i]);
            child->parent = new_node.get();
            new_node->children.push_back(std::move(child));
        }
        node->children.resize(mid + 1);

        insertIntoParent(node, up_key, std::move(new_node));
    }

    void insertIntoParent(Node* left, const Key& key, std::unique_ptr<Node> right) {
        if (!left->parent) {
            auto new_root = std::make_unique<Node>(false);
            new_root->keys.push_back(key);
            new_root->children.push_back(std::move(root_));
            new_root->children.push_back(std::move(right));
            new_root->children[0]->parent = new_root.get();
            new_root->children[1]->parent = new_root.get();
            root_ = std::move(new_root);
            return;
        }

        Node* parent = left->parent;
        Node* right_raw = right.get();
        right_raw->parent = parent;

        auto pos = std::find_if(parent->children.begin(), parent->children.end(),
                                [left](const std::unique_ptr<Node>& child) { return child.get() == left; });
        if (pos == parent->children.end()) {
            throw std::logic_error("Broken parent/child relationship in B+Tree");
        }
        std::size_t index = static_cast<std::size_t>(std::distance(parent->children.begin(), pos));

        parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(index), key);
        parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));

        if (parent->keys.size() > maxKeys()) {
            splitInternal(parent);
        }
    }

    void updateParentKeyForChild(Node* child) {
        if (!child->parent) {
            return;
        }
        Node* parent = child->parent;
        std::size_t idx = childIndex(parent, child);
        if (idx == 0) {
            return;  // The first child is unconstrained by parent keys
        }
        parent->keys[idx - 1] = child->keys.front();
    }

    std::size_t childIndex(const Node* parent, const Node* child) const {
        auto it = std::find_if(parent->children.begin(), parent->children.end(),
                               [child](const std::unique_ptr<Node>& candidate) { return candidate.get() == child; });
        if (it == parent->children.end()) {
            throw std::logic_error("Child missing from parent in B+Tree");
        }
        return static_cast<std::size_t>(std::distance(parent->children.begin(), it));
    }

    void rebalanceAfterDeletion(Node* node) {
        if (node == root_.get()) {
            if (!node->leaf && node->keys.empty()) {
                std::unique_ptr<Node> new_root = std::move(root_->children.front());
                root_->children.clear();
                root_ = std::move(new_root);
                if (root_) {
                    root_->parent = nullptr;
                }
            }
            return;
        }

        Node* parent = node->parent;
        std::size_t idx = childIndex(parent, node);

        if (node->leaf) {
            if (node->keys.size() >= minKeysLeaf()) {
                if (!node->keys.empty()) {
                    updateParentKeyForChild(node);
                }
                return;
            }

            Node* left = idx > 0 ? parent->children[idx - 1].get() : nullptr;
            Node* right = (idx + 1 < parent->children.size()) ? parent->children[idx + 1].get() : nullptr;

            if (left && left->keys.size() > minKeysLeaf()) {
                borrowFromLeftLeaf(node, left, idx - 1);
                updateParentKeyForChild(node);
                return;
            }
            if (right && right->keys.size() > minKeysLeaf()) {
                borrowFromRightLeaf(node, right, idx);
                parent->keys[idx] = right->keys.front();
                return;
            }

            if (left) {
                mergeLeaves(left, node, idx - 1);
                node = left;
            } else if (right) {
                mergeLeaves(node, right, idx);
            }

            if (parent == root_.get() && parent->keys.empty()) {
                std::unique_ptr<Node> new_root = std::move(root_->children.front());
                root_->children.clear();
                root_ = std::move(new_root);
                if (root_) {
                    root_->parent = nullptr;
                }
                return;
            }

            if (parent->keys.size() < minKeysInternal()) {
                rebalanceAfterDeletion(parent);
            }
            return;
        }

        // Internal node
        if (node->keys.size() >= minKeysInternal()) {
            return;
        }

        Node* left = idx > 0 ? parent->children[idx - 1].get() : nullptr;
        Node* right = (idx + 1 < parent->children.size()) ? parent->children[idx + 1].get() : nullptr;

        if (left && left->keys.size() > minKeysInternal()) {
            borrowFromLeftInternal(node, left, idx - 1);
            return;
        }
        if (right && right->keys.size() > minKeysInternal()) {
            borrowFromRightInternal(node, right, idx);
            return;
        }

        if (left) {
            mergeInternal(left, node, idx - 1);
            node = left;
        } else if (right) {
            mergeInternal(node, right, idx);
        }

        if (parent == root_.get() && parent->keys.empty()) {
            std::unique_ptr<Node> new_root = std::move(root_->children.front());
            root_->children.clear();
            root_ = std::move(new_root);
            if (root_) {
                root_->parent = nullptr;
            }
            return;
        }

        if (parent->keys.size() < minKeysInternal()) {
            rebalanceAfterDeletion(parent);
        }
    }

    void borrowFromLeftLeaf(Node* node, Node* left, std::size_t parent_index) {
        node->keys.insert(node->keys.begin(), left->keys.back());
        node->values.insert(node->values.begin(), left->values.back());
        left->keys.pop_back();
        left->values.pop_back();
        node->parent->keys[parent_index] = node->keys.front();
    }

    void borrowFromRightLeaf(Node* node, Node* right, std::size_t parent_index) {
        node->keys.push_back(right->keys.front());
        node->values.push_back(right->values.front());
        right->keys.erase(right->keys.begin());
        right->values.erase(right->values.begin());
        node->parent->keys[parent_index] = right->keys.front();
    }

    void mergeLeaves(Node* left, Node* right, std::size_t parent_index) {
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
        left->values.insert(left->values.end(), right->values.begin(), right->values.end());
        left->next = right->next;
        if (right->next) {
            right->next->prev = left;
        }

        Node* parent = left->parent;
        parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(parent_index));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(parent_index + 1));
    }

    void borrowFromLeftInternal(Node* node, Node* left, std::size_t parent_index) {
        node->keys.insert(node->keys.begin(), node->parent->keys[parent_index]);
        node->parent->keys[parent_index] = left->keys.back();
        left->keys.pop_back();

        std::unique_ptr<Node> child = std::move(left->children.back());
        left->children.pop_back();
        child->parent = node;
        node->children.insert(node->children.begin(), std::move(child));
    }

    void borrowFromRightInternal(Node* node, Node* right, std::size_t parent_index) {
        node->keys.push_back(node->parent->keys[parent_index]);
        node->parent->keys[parent_index] = right->keys.front();
        right->keys.erase(right->keys.begin());

        std::unique_ptr<Node> child = std::move(right->children.front());
        right->children.erase(right->children.begin());
        child->parent = node;
        node->children.push_back(std::move(child));
    }

    void mergeInternal(Node* left, Node* right, std::size_t parent_index) {
        Node* parent = left->parent;
        left->keys.push_back(parent->keys[parent_index]);
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());

        for (auto& child : right->children) {
            child->parent = left;
            left->children.push_back(std::move(child));
        }
        right->children.clear();

        parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(parent_index));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(parent_index + 1));
    }

    std::unique_ptr<Node> root_;
};

#ifdef B_PLUS_TREE_DEMO
#include <iostream>
#include <string>

int main() {
    BPlusTree<std::string, std::string, 4> tree;
    tree.insert("A", "ten");
    tree.insert("B", "twenty");
    tree.insert("C", "five");

    std::cout << "A => " << *tree.find("A") << '\n';
    std::cout << "B => " << *tree.find("B") << '\n';
    std::cout << "C => " << *tree.find("C") << '\n';

    return 0;
}
#endif
