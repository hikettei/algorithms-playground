CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Werror -g

DEMO_BIN := b_plus_tree_demo
TEST_BIN := b_plus_tree_tests

.PHONY: all demo test clean run-test

all: demo test

demo: main.cpp
	$(CXX) $(CXXFLAGS) -DB_PLUS_TREE_DEMO main.cpp -o $(DEMO_BIN)

test: test.cpp main.cpp
	$(CXX) $(CXXFLAGS) test.cpp -o $(TEST_BIN)

run-test: test
	./$(TEST_BIN)

clean:
	$(RM) $(DEMO_BIN) $(TEST_BIN)
