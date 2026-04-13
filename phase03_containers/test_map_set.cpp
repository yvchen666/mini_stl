#include <gtest/gtest.h>
#include "map_set.h"
#include <string>

// ==================== set ====================
TEST(Set, InsertFind) {
    mystl::set<int> s = {3, 1, 4, 1, 5, 9};
    // 去重后应有 5 个元素
    EXPECT_EQ(s.size(), 5u);
    EXPECT_NE(s.find(3), s.end());
    EXPECT_EQ(s.find(99), s.end());
}

TEST(Set, Sorted) {
    mystl::set<int> s = {5, 3, 8, 1, 7};
    int prev = *s.begin();
    for (auto it = ++s.begin(); it != s.end(); ++it) {
        EXPECT_LT(prev, *it);
        prev = *it;
    }
}

TEST(Set, Erase) {
    mystl::set<int> s = {1, 2, 3, 4, 5};
    s.erase(3);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.find(3), s.end());
}

TEST(Set, LowerUpperBound) {
    mystl::set<int> s = {1, 3, 5, 7, 9};
    EXPECT_EQ(*s.lower_bound(4), 5);
    EXPECT_EQ(*s.upper_bound(5), 7);
}

// ==================== multiset ====================
TEST(Multiset, AllowsDuplicates) {
    mystl::multiset<int> ms = {1, 2, 2, 3, 3, 3};
    EXPECT_EQ(ms.size(), 6u);
    EXPECT_EQ(ms.count(3), 3u);
}

// ==================== map ====================
TEST(Map, InsertFind) {
    mystl::map<std::string, int> m;
    m.insert({"apple", 1});
    m.insert({"banana", 2});
    m.insert({"cherry", 3});
    EXPECT_EQ(m.size(), 3u);
    auto it = m.find("banana");
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->second, 2);
    EXPECT_EQ(m.find("durian"), m.end());
}

TEST(Map, OperatorBracket_Insert) {
    mystl::map<std::string, int> m;
    m["a"] = 1;
    m["b"] = 2;
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m["a"], 1);
    // 访问不存在的 key 应插入默认值
    int v = m["c"];
    EXPECT_EQ(v, 0);
    EXPECT_EQ(m.size(), 3u);
}

TEST(Map, OperatorBracket_Update) {
    mystl::map<std::string, int> m;
    m["x"] = 10;
    m["x"] = 20;
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m["x"], 20);
}

TEST(Map, At_Throws) {
    mystl::map<int, int> m;
    m[1] = 10;
    EXPECT_EQ(m.at(1), 10);
    EXPECT_THROW(m.at(99), std::out_of_range);
}

TEST(Map, Sorted) {
    mystl::map<int, std::string> m;
    m[3] = "c"; m[1] = "a"; m[2] = "b";
    int prev_key = -1;
    for (const auto& p : m) {
        EXPECT_GT(p.first, prev_key);
        prev_key = p.first;
    }
}

TEST(Map, Erase) {
    mystl::map<int, int> m;
    for (int i = 1; i <= 5; ++i) m[i] = i * 10;
    m.erase(3);
    EXPECT_EQ(m.size(), 4u);
    EXPECT_EQ(m.find(3), m.end());
}

// ==================== multimap ====================
TEST(Multimap, AllowsDuplicateKeys) {
    mystl::multimap<int, std::string> mm;
    mm.insert({1, "a"}); mm.insert({1, "b"}); mm.insert({2, "c"});
    EXPECT_EQ(mm.size(), 3u);
    EXPECT_EQ(mm.count(1), 2u);
}
