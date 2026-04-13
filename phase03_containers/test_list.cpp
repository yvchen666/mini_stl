#include <gtest/gtest.h>
#include "list.h"
#include <string>
#include <algorithm>

using mystl::list;

TEST(List, DefaultEmpty) {
    list<int> l;
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
}

TEST(List, PushBack) {
    list<int> l;
    l.push_back(1); l.push_back(2); l.push_back(3);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 3);
}

TEST(List, PushFront) {
    list<int> l;
    l.push_front(3); l.push_front(2); l.push_front(1);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 3);
}

TEST(List, PopFrontBack) {
    list<int> l = {1, 2, 3, 4, 5};
    l.pop_front();
    EXPECT_EQ(l.front(), 2);
    l.pop_back();
    EXPECT_EQ(l.back(), 4);
    EXPECT_EQ(l.size(), 3u);
}

TEST(List, Iterate) {
    list<int> l = {10, 20, 30};
    int expected[] = {10, 20, 30};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(List, InsertErase) {
    list<int> l = {1, 3, 5};
    auto it = l.begin(); ++it;
    l.insert(it, 2);  // {1,2,3,5}
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(*l.begin(), 1);

    it = l.begin(); ++it; ++it;  // 指向 3
    l.erase(it);  // {1,2,5}
    EXPECT_EQ(l.size(), 3u);
}

TEST(List, CopyConstruct) {
    list<std::string> a = {"a", "b", "c"};
    list<std::string> b = a;
    EXPECT_EQ(a, b);
    b.front() = "x";
    EXPECT_NE(a.front(), "x");  // 深拷贝
}

TEST(List, MoveConstruct) {
    list<int> a = {1, 2, 3};
    list<int> b = std::move(a);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_TRUE(a.empty());
}

TEST(List, Splice) {
    list<int> a = {1, 2, 3};
    list<int> b = {4, 5, 6};
    a.splice(a.end(), b);
    EXPECT_EQ(a.size(), 6u);
    EXPECT_TRUE(b.empty());
    int expected[] = {1, 2, 3, 4, 5, 6};
    int i = 0;
    for (auto v : a) EXPECT_EQ(v, expected[i++]);
}

TEST(List, Remove) {
    list<int> l = {1, 2, 3, 2, 4};
    l.remove(2);
    EXPECT_EQ(l.size(), 3u);
    for (auto v : l) EXPECT_NE(v, 2);
}

TEST(List, Unique) {
    list<int> l = {1, 1, 2, 3, 3, 3, 4};
    l.unique();
    EXPECT_EQ(l.size(), 4u);
    int expected[] = {1, 2, 3, 4};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(List, Sort) {
    list<int> l = {5, 3, 1, 4, 2};
    l.sort();
    int expected[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(List, ReverseIteration) {
    list<int> l = {1, 2, 3, 4, 5};
    std::vector<int> reversed;
    for (auto it = l.rbegin(); it != l.rend(); ++it)
        reversed.push_back(*it);
    EXPECT_EQ(reversed[0], 5);
    EXPECT_EQ(reversed[4], 1);
}
