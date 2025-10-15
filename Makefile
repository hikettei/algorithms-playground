CXX := clang++

b_plus_tree:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -DB_PLUS_TREE_DEMO ./b_plus_tree.cpp -o b_plus_tree_demo
