#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "main.cpp"

namespace test {
namespace {
struct Context {
    int total_checks = 0;
    int failed_checks = 0;
} ctx;

thread_local const char* current_test = "(unknown)";

[[maybe_unused]] std::string debugString(const std::string& value) {
    return std::string("\"") + value + "\"";
}

[[maybe_unused]] std::string debugString(bool value) { return value ? "true" : "false"; }

template <typename T>
std::string debugString(const std::optional<T>& opt);

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
std::string debugString(const T& value) {
    std::ostringstream oss;
    oss << value;
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

void testBasicInsertFind() {
    test::TestScope scope("basic_insert_find");
    BPlusTree<int, std::string, 4> tree;

    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto value = tree.find(2);
    CHECK_TRUE(value.has_value());
    CHECK_EQ(*value, std::string("two"));

    auto missing = tree.find(99);
    CHECK_FALSE(missing.has_value());
}

void testOverwriteValue() {
    test::TestScope scope("overwrite_value");
    BPlusTree<std::string, int, 5> tree;

    tree.insert("alpha", 1);
    tree.insert("beta", 2);
    tree.insert("alpha", 42);

    auto first = tree.find("alpha");
    CHECK_TRUE(first.has_value());
    CHECK_EQ(*first, 42);

    auto beta = tree.find("beta");
    CHECK_TRUE(beta.has_value());
    CHECK_EQ(*beta, 2);
}

void testSequentialBulkInsert() {
    test::TestScope scope("sequential_bulk_insert");
    constexpr int kCount = 50'000;
    BPlusTree<int, int, 8> tree;

    for (int key = 0; key < kCount; ++key) {
        tree.insert(key, key * 3 + 7);
    }

    for (int key = 0; key < kCount; ++key) {
        auto value = tree.find(key);
        CHECK_TRUE(value.has_value());
        CHECK_EQ(*value, key * 3 + 7);
    }

    for (int key = 1; key <= kCount; key *= 2) {
        auto missing = tree.find(-key);
        CHECK_FALSE(missing.has_value());
    }
}

void testRandomBulkInsert() {
    test::TestScope scope("random_bulk_insert");
    constexpr int kIterations = 40'000;
    BPlusTree<int, std::int64_t, 6> tree;
    std::unordered_map<int, std::int64_t> reference;
    reference.reserve(static_cast<std::size_t>(kIterations));

    std::mt19937 rng(0xF17E1u);
    std::uniform_int_distribution<int> key_dist(-500'000, 500'000);
    std::uniform_int_distribution<std::int64_t> value_dist(-1'000'000'000LL, 1'000'000'000LL);

    for (int i = 0; i < kIterations; ++i) {
        int key = key_dist(rng);
        std::int64_t value = value_dist(rng);
        tree.insert(key, value);
        reference[key] = value;
    }

    for (const auto& [key, expected] : reference) {
        auto actual = tree.find(key);
        CHECK_TRUE(actual.has_value());
        CHECK_EQ(*actual, expected);
    }

    for (int i = 0; i < 4'000; ++i) {
        int key = key_dist(rng);
        if (reference.find(key) != reference.end()) {
            continue;
        }
        auto result = tree.find(key);
        CHECK_FALSE(result.has_value());
    }
}

void testInterleavedInsertFind() {
    test::TestScope scope("interleaved_insert_find");
    constexpr int kOperations = 30'000;
    BPlusTree<int, int, 4> tree;
    std::unordered_map<int, int> reference;
    reference.reserve(static_cast<std::size_t>(kOperations));
    std::vector<int> known_keys;
    known_keys.reserve(static_cast<std::size_t>(kOperations));

    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> key_dist(-100'000, 100'000);
    std::uniform_int_distribution<int> value_dist(-1'000'000, 1'000'000);

    for (int op = 0; op < kOperations; ++op) {
        int key = key_dist(rng);
        int value = value_dist(rng);
        tree.insert(key, value);
        if (reference.insert_or_assign(key, value).second) {
            known_keys.push_back(key);
        }

        if (!known_keys.empty()) {
            for (int probe = 0; probe < 3; ++probe) {
                const int sampled_index = static_cast<int>(rng() % known_keys.size());
                int probe_key = known_keys[static_cast<std::size_t>(sampled_index)];
                auto actual = tree.find(probe_key);
                CHECK_TRUE(actual.has_value());
                CHECK_EQ(*actual, reference[probe_key]);
            }
        }
    }

    for (int key : known_keys) {
        auto actual = tree.find(key);
        CHECK_TRUE(actual.has_value());
        CHECK_EQ(*actual, reference[key]);
    }
}

int main() {
    testBasicInsertFind();
    testOverwriteValue();
    testSequentialBulkInsert();
    testRandomBulkInsert();
    testInterleavedInsertFind();
    return ::test::finalize();
}
