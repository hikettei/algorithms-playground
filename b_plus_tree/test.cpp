#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "main.cpp"

namespace test {
namespace {
struct Context {
    int total_checks = 0;
    int failed_checks = 0;
} ctx;

thread_local const char* current_test = "(unknown)";

inline std::string debugString(const std::string& value) {
    return std::string("\"") + value + "\"";
}

inline std::string debugString(bool value) {
    return value ? "true" : "false";
}

template <typename T>
std::string debugString(const std::optional<T>& opt);

template <typename T>
std::string debugString(const std::vector<T>& vec);

template <typename T>
std::string debugString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

template <typename T>
std::string debugString(const std::vector<T>& vec) {
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) {
            oss << ", ";
        }
        oss << debugString(vec[i]);
    }
    oss << ']';
    return oss.str();
}

template <typename T>
std::string debugString(const std::optional<T>& opt) {
    if (!opt.has_value()) {
        return "nullopt";
    }
    return std::string("optional(") + debugString(*opt) + std::string(")");
}

void reportFailure(const char* expression, const char* file, int line, const std::string& details) {
    ++ctx.failed_checks;
    std::cerr << "[" << current_test << "] " << file << ':' << line << " -- " << expression;
    if (!details.empty()) {
        std::cerr << " :: " << details;
    }
    std::cerr << '\n';
}
}  // namespace

class TestScope {
public:
    explicit TestScope(const char* name) : previous_(current_test) { current_test = name; }
    ~TestScope() { current_test = previous_; }

private:
    const char* previous_;
};

void checkTrue(bool condition, const char* expression, const char* file, int line) {
    ++ctx.total_checks;
    if (!condition) {
        reportFailure(expression, file, line, "expected true but got false");
    }
}

void checkFalse(bool condition, const char* expression, const char* file, int line) {
    ++ctx.total_checks;
    if (condition) {
        reportFailure(expression, file, line, "expected false but got true");
    }
}

template <typename T, typename U>
void checkEqual(const T& actual, const U& expected, const char* actual_expr, const char* expected_expr, const char* file, int line) {
    ++ctx.total_checks;
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << "expected " << expected_expr << " == " << actual_expr
            << " but \n  expected: " << debugString(expected)
            << "\n    actual: " << debugString(actual);
        reportFailure("CHECK_EQ", file, line, oss.str());
    }
}

int finalize() {
    if (ctx.failed_checks == 0) {
        std::cout << "All " << ctx.total_checks << " checks passed." << std::endl;
        return 0;
    }
    std::cerr << ctx.failed_checks << " / " << ctx.total_checks << " checks failed." << std::endl;
    return 1;
}

}  // namespace test

#define CHECK_TRUE(expr) ::test::checkTrue((expr), #expr, __FILE__, __LINE__)
#define CHECK_FALSE(expr) ::test::checkFalse((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) ::test::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

template <typename Key, typename Value>
std::vector<Value> collectRange(const std::map<Key, Value>& reference, const Key& start, const Key& end) {
    std::vector<Value> out;
    if (start > end) {
        return out;
    }
    auto it = reference.lower_bound(start);
    while (it != reference.end() && it->first <= end) {
        out.push_back(it->second);
        ++it;
    }
    return out;
}

void testBasicInsertionAndLookup() {
    test::TestScope scope("basic_insertion_and_lookup");
    BPlusTree<int, std::string, 4> tree;
    CHECK_TRUE(tree.empty());

    const std::vector<std::pair<int, std::string>> data = {
        {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
    };
    for (const auto& [key, value] : data) {
        tree.insert(key, value);
    }

    CHECK_FALSE(tree.empty());

    auto two = tree.find(2);
    CHECK_TRUE(two.has_value());
    CHECK_EQ(*two, std::string("two"));

    auto missing = tree.find(42);
    CHECK_FALSE(missing.has_value());

    auto values = tree.range(1, 5);
    CHECK_EQ(values, (std::vector<std::string>{"one", "two", "three", "four", "five"}));

    auto partial = tree.range(3, 4);
    CHECK_EQ(partial, (std::vector<std::string>{"three", "four"}));

    auto empty_range = tree.range(10, 5);
    CHECK_TRUE(empty_range.empty());
}

void testOverwriteExistingKey() {
    test::TestScope scope("overwrite_existing_key");
    BPlusTree<int, std::string, 4> tree;

    tree.insert(42, "alpha");
    tree.insert(42, "beta");
    auto value = tree.find(42);
    CHECK_TRUE(value.has_value());
    CHECK_EQ(*value, std::string("beta"));

    auto range = tree.range(40, 50);
    CHECK_EQ(range, (std::vector<std::string>{"beta"}));

    CHECK_TRUE(tree.erase(42));
    CHECK_FALSE(tree.erase(42));
    CHECK_TRUE(tree.range(42, 42).empty());
}

void testRangeQueriesAcrossLeaves() {
    test::TestScope scope("range_queries_across_leaves");
    BPlusTree<int, int, 4> tree;

    for (int key = 0; key < 24; ++key) {
        tree.insert(key, key * 10);
    }

    auto mid = tree.range(3, 12);
    CHECK_EQ(mid, (std::vector<int>{30, 40, 50, 60, 70, 80, 90, 100, 110, 120}));

    auto tail = tree.range(18, 30);
    CHECK_EQ(tail, (std::vector<int>{180, 190, 200, 210, 220, 230}));

    auto none = tree.range(-5, -1);
    CHECK_TRUE(none.empty());

    auto single = tree.range(7, 7);
    CHECK_EQ(single, (std::vector<int>{70}));
}

void testEraseAndRebalance() {
    test::TestScope scope("erase_and_rebalance");
    BPlusTree<int, int, 4> tree;
    std::map<int, int> reference;

    for (int key = 0; key < 30; ++key) {
        tree.insert(key, key + 100);
        reference.emplace(key, key + 100);
    }

    for (int key : {0, 1, 2, 3, 4, 5, 6, 16, 17, 18}) {
        CHECK_TRUE(tree.erase(key));
        reference.erase(key);
    }

    CHECK_FALSE(tree.erase(50));

    for (int key = 0; key < 30; ++key) {
        auto expected = reference.find(key);
        auto actual = tree.find(key);
        if (expected == reference.end()) {
            CHECK_FALSE(actual.has_value());
        } else {
            CHECK_TRUE(actual.has_value());
            CHECK_EQ(*actual, expected->second);
        }
    }

    auto full_range = tree.range(0, 29);
    auto expected_range = collectRange(reference, 0, 29);
    CHECK_EQ(full_range, expected_range);

    // Remove remaining keys to ensure tree collapses back to a leaf root.
    for (int key = 29; key >= 7; --key) {
        if (reference.erase(key) > 0) {
            CHECK_TRUE(tree.erase(key));
        }
    }
    CHECK_FALSE(tree.erase(10));
    CHECK_TRUE(tree.empty());
}

void testClearAndReuse() {
    test::TestScope scope("clear_and_reuse");
    BPlusTree<std::string, int, 5> tree;

    tree.insert("delta", 4);
    tree.insert("alpha", 1);
    tree.insert("charlie", 3);
    tree.insert("bravo", 2);

    CHECK_FALSE(tree.empty());
    tree.clear();
    CHECK_TRUE(tree.empty());
    CHECK_FALSE(tree.erase("alpha"));

    tree.insert("echo", 5);
    tree.insert("foxtrot", 6);

    auto value = tree.find("echo");
    CHECK_TRUE(value.has_value());
    CHECK_EQ(*value, 5);

    auto ordered = tree.range("alpha", "zulu");
    CHECK_EQ(ordered, (std::vector<int>{5, 6}));
}

void testRandomizedOperations() {
    test::TestScope scope("randomized_operations");
    BPlusTree<int, int, 4> tree;
    std::map<int, int> reference;
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> key_dist(-256, 256);
    std::uniform_int_distribution<int> value_dist(-1000, 1000);

    constexpr int kIterations = 4000;
    for (int step = 0; step < kIterations; ++step) {
        int action = static_cast<int>(rng() % 3);
        int key = key_dist(rng);
        if (action == 0) {
            int value = value_dist(rng);
            tree.insert(key, value);
            reference[key] = value;
        } else if (action == 1) {
            bool removed_tree = tree.erase(key);
            bool removed_reference = reference.erase(key) > 0;
            CHECK_EQ(removed_tree, removed_reference);
        } else {
            auto actual = tree.find(key);
            auto expected = reference.find(key);
            CHECK_EQ(actual.has_value(), expected != reference.end());
            if (expected != reference.end()) {
                CHECK_TRUE(actual.has_value());
                CHECK_EQ(*actual, expected->second);
            }
        }

        int range_start = key_dist(rng);
        int range_end = key_dist(rng);
        if (range_start > range_end) {
            std::swap(range_start, range_end);
        }
        auto expected_range = collectRange(reference, range_start, range_end);
        auto actual_range = tree.range(range_start, range_end);
        CHECK_EQ(actual_range, expected_range);

        CHECK_EQ(tree.empty(), reference.empty());
        CHECK_TRUE(tree.range(5, 3).empty());
    }

    tree.clear();
    reference.clear();
    CHECK_TRUE(tree.empty());
    CHECK_TRUE(tree.range(-10, 10).empty());
}

int main() {
    testBasicInsertionAndLookup();
    testOverwriteExistingKey();
    testRangeQueriesAcrossLeaves();
    testEraseAndRebalance();
    testClearAndReuse();
    testRandomizedOperations();
    return ::test::finalize();
}
