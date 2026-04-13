#include <gtest/gtest.h>
#include "hashtable.h"
#include <string>

// ==================== hash_set ====================
TEST(HashSet, InsertFind) {
    mystl::hash_set<int> s;
    s.insert(1); s.insert(2); s.insert(3);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_NE(s.find(2), s.end());
    EXPECT_EQ(s.find(99), s.end());
}

TEST(HashSet, NoDuplicates) {
    mystl::hash_set<int> s;
    for (int i = 0; i < 3; ++i) s.insert(42);  // 同一值插入 3 次
    EXPECT_EQ(s.size(), 1u);
}

TEST(HashSet, InitList) {
    mystl::hash_set<int> s = {1, 2, 3, 2, 1};
    EXPECT_EQ(s.size(), 3u);
}

TEST(HashSet, Erase) {
    mystl::hash_set<int> s = {1, 2, 3, 4, 5};
    s.erase(3);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.find(3), s.end());
}

TEST(HashSet, StringKeys) {
    mystl::hash_set<std::string> s;
    s.insert("hello"); s.insert("world"); s.insert("foo");
    EXPECT_EQ(s.size(), 3u);
    EXPECT_NE(s.find("hello"), s.end());
    EXPECT_EQ(s.find("bar"), s.end());
}

TEST(HashSet, Rehash) {
    // 插入大量元素，触发 rehash
    mystl::hash_set<int> s;
    for (int i = 0; i < 200; ++i) s.insert(i);
    EXPECT_EQ(s.size(), 200u);
    for (int i = 0; i < 200; ++i) EXPECT_NE(s.find(i), s.end());
}

// ==================== hash_map ====================
TEST(HashMap, InsertFind) {
    mystl::hash_map<std::string, int> m;
    m.insert({"a", 1}); m.insert({"b", 2});
    EXPECT_EQ(m.size(), 2u);
    auto it = m.find("a");
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->second, 1);
}

TEST(HashMap, OperatorBracket) {
    mystl::hash_map<std::string, int> m;
    m["x"] = 10; m["y"] = 20;
    EXPECT_EQ(m["x"], 10);
    EXPECT_EQ(m["y"], 20);
    // 访问不存在的 key 插入默认值
    int v = m["z"];
    EXPECT_EQ(v, 0);
    EXPECT_EQ(m.size(), 3u);
}

TEST(HashMap, WordCount) {
    mystl::hash_map<std::string, int> wc;
    for (const char* w : {"a", "b", "a", "c", "b", "a"})
        ++wc[w];
    EXPECT_EQ(wc["a"], 3);
    EXPECT_EQ(wc["b"], 2);
    EXPECT_EQ(wc["c"], 1);
}

TEST(HashMap, Iterate) {
    mystl::hash_map<int, int> m;
    for (int i = 1; i <= 5; ++i) m[i] = i * i;
    int sum = 0;
    for (auto it = m.begin(); it != m.end(); ++it)
        sum += it->second;
    // 1 + 4 + 9 + 16 + 25 = 55
    EXPECT_EQ(sum, 55);
}
