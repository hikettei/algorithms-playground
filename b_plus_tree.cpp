#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename Key, typename Value, std::size_t Order>
class BPlusTree {
  struct Node {
    explicit Node(bool is_leaf) : leaf(is_leaf), parent(nullptr) ,predecssors(nullptr), successors(nullptr) {}
    bool leaf;
    std::vector<Key> keys;
    std::vector<Value> values; // Valid when leaf
    std::vector<std::unique_ptr<Node>> children; // Valid when itnernal
    Node* parent;
    Node* predecessors;
    Node* successors;
  };

public:
  using key_type = Key;
  using mapped_type = Value;
  BPlusTree() : root_(std::make_unique<Node>(true)) {}
  bool empty() const nonexcept { return root_->leaf && root->keys.empty(); }
private:
  
}

#ifdef B_PLUS_TREE_DEMO
#include <iostream>
#include <string>

int main() {
    BPlusTree<int, std::string, 4> tree;
    tree.insert(10, "ten");
    tree.insert(20, "twenty");
    tree.insert(5, "five");

    auto value = tree.find(10);
    if (value) {
        std::cout << "10 => " << *value << '\n';
    }

    for (const auto& v : tree.range(0, 30)) {
        std::cout << v << '\n';
    }

    tree.erase(10);
    std::cout << (tree.find(10) ? "still there" : "removed") << '\n';
    return 0;
}
#endif
