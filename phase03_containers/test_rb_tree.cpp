#include <gtest/gtest.h>
#include "rb_tree.h"
#include <string>
#include <vector>
#include <algorithm>

// 测试 rb_tree 直接使用（key == value，用 Identity）
using IntTree = mystl::rb_tree<int, int, mystl::Identity<int>>;

// ==================== 基本插入与迭代 ====================
TEST(RBTree, InsertUnique_Sorted) {
    IntTree t;
    for (int v : {5, 3, 7, 1, 4, 6, 8}) t.insert_unique(v);
    EXPECT_EQ(t.size(), 7u);
    // 中序遍历应得到有序序列
    std::vector<int> result;
    for (auto it = t.begin(); it != t.end(); ++it)
        result.push_back(*it);
    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[6], 8);
}

TEST(RBTree, InsertEqual_Duplicates) {
    IntTree t;
    t.insert_equal(3);
    t.insert_equal(3);
    t.insert_equal(3);
    EXPECT_EQ(t.size(), 3u);
    EXPECT_EQ(t.count(3), 3u);
}

TEST(RBTree, InsertUnique_Duplicate_Rejected) {
    IntTree t;
    auto [it1, ok1] = t.insert_unique(5);
    auto [it2, ok2] = t.insert_unique(5);
    EXPECT_TRUE(ok1);
    EXPECT_FALSE(ok2);
    EXPECT_EQ(t.size(), 1u);
}

// ==================== 查找 ====================
TEST(RBTree, Find) {
    IntTree t;
    for (int i = 1; i <= 10; ++i) t.insert_unique(i);
    auto it = t.find(5);
    EXPECT_NE(it, t.end());
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(t.find(99), t.end());
}

TEST(RBTree, LowerUpperBound) {
    IntTree t;
    for (int i : {1, 3, 5, 7, 9}) t.insert_unique(i);
    auto lb = t.lower_bound(4);
    EXPECT_EQ(*lb, 5);
    auto ub = t.upper_bound(5);
    EXPECT_EQ(*ub, 7);
}

// ==================== 删除 ====================
TEST(RBTree, Erase_Leaf) {
    IntTree t;
    for (int i : {5, 3, 7}) t.insert_unique(i);
    t.erase(3);
    EXPECT_EQ(t.size(), 2u);
    EXPECT_EQ(t.find(3), t.end());
}

TEST(RBTree, Erase_Internal) {
    IntTree t;
    for (int i : {5, 3, 7, 1, 4}) t.insert_unique(i);
    t.erase(3);  // 有两个子节点
    EXPECT_EQ(t.size(), 4u);
    EXPECT_EQ(t.find(3), t.end());
    // 剩余元素仍有序
    std::vector<int> result;
    for (auto v : t) result.push_back(v);
    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));
}

TEST(RBTree, Erase_ByKey) {
    IntTree t;
    for (int i = 1; i <= 10; ++i) t.insert_unique(i);
    std::size_t n = t.erase(5);
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(t.size(), 9u);
}

// ==================== 大规模正确性测试 ====================
TEST(RBTree, LargeInsertDelete) {
    IntTree t;
    for (int i = 100; i >= 1; --i) t.insert_unique(i);
    EXPECT_EQ(t.size(), 100u);
    // 验证有序
    std::vector<int> result;
    for (auto v : t) result.push_back(v);
    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));
    EXPECT_EQ(result.front(), 1);
    EXPECT_EQ(result.back(), 100);
    // 删除偶数
    for (int i = 2; i <= 100; i += 2) t.erase(i);
    EXPECT_EQ(t.size(), 50u);
    for (auto v : t) EXPECT_EQ(v % 2, 1);  // 只剩奇数
}

TEST(RBTree, Clear) {
    IntTree t;
    for (int i = 0; i < 20; ++i) t.insert_unique(i);
    t.clear();
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.begin(), t.end());
}

TEST(RBTree, ReverseIteration) {
    IntTree t;
    for (int i : {3, 1, 4, 1, 5, 9, 2, 6}) t.insert_unique(i);
    std::vector<int> fwd, rev;
    for (auto it = t.begin();  it != t.end();  ++it) fwd.push_back(*it);
    for (auto it = t.end();    it != t.begin(); ) { --it; rev.push_back(*it); }
    EXPECT_EQ(fwd.size(), rev.size());
    std::reverse(rev.begin(), rev.end());
    EXPECT_EQ(fwd, rev);
}
