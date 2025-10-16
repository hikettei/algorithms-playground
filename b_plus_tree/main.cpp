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
  /*
    Data Structure:
    ```
           [ Root (internal) ]
               /       \
          [ I ]         [ I ]        ← internal nodes
           / \           / \
        [L0] [L1]     [L2] [L3]      ← leaves (where leaf = true)
        L0.next == L1, L1.next == L2, ... (as well as prev)
   ```
   - values are assigned only if the node is LeafNode.
   - LeafNodes never have children.
   - parent = Bucket_{lvl_1}
  */
  struct Node {
    explicit Node(bool is_leaf) : leaf(is_leaf), parent(nullptr) {}
    bool leaf;
    std::vector<Key> keys;
    std::vector<Value> values; // Valid only when the node is leaf
    std::vector<std::unique_ptr<Node>> children; // Valid when the node is internal
    Node* parent;
  };
  std::unique_ptr<Node> root_;

public:
  using key_type = Key;
  using mapped_type = Value;
  BPlusTree() : root_(std::make_unique<Node>(true)) {}
  void insert(const Key& key, const Value& value) {
    Node* leaf = findLeaf(key);

    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    const std::size_t index = static_cast<std::size_t>(std::distance(leaf->keys.begin(), it));
    
    if (it != leaf->keys.end() && *it == key) { // overriding the key.
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
private:
    static constexpr std::size_t maxKeys() { return Order - 1; }
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
        insertIntoParent(leaf, new_leaf->keys.front(), std::move(new_leaf));
    }

    void splitInternal(Node* node) {
        auto new_node = std::make_unique<Node>(false);
        std::size_t mid = node->keys.size() / 2;
        Key up_key = node->keys[mid];

        new_node->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), node->keys.end());
        node->keys.resize(mid);
        // split and distribute children
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
        if (!child->parent) return;
        Node* parent = child->parent;
        std::size_t idx = childIndex(parent, child);
        if (idx == 0) return; // The first child is unconstrained by parent keys
        parent->keys[idx - 1] = child->keys.front();
    }

    std::size_t childIndex(const Node* parent, const Node* child) const {
        auto it = std::find_if(parent->children.begin(), parent->children.end(),
                               [child](const std::unique_ptr<Node>& candidate) { return candidate.get() == child; });
        if (it == parent->children.end()) throw std::logic_error("Child missing from parent in B+Tree");
        return static_cast<std::size_t>(std::distance(parent->children.begin(), it));
    }
   
};

#ifdef B_PLUS_TREE_DEMO
#include <iostream>
#include <string>

int main() {
    BPlusTree<std::string, std::string, 4> tree;
    tree.insert("B", "ten");
    tree.insert("A", "twenty");
    tree.insert("C", "five");

    std::cout << "A => " << *tree.find("A") << '\n';
    std::cout << "B => " << *tree.find("B") << '\n';
    std::cout << "C => " << *tree.find("C") << '\n';

    return 0;
}
#endif
