#include <gtest/gtest.h>
#include "vector.h"
#include <string>
#include <numeric>
#include <algorithm>

using mystl::vector;

// ==================== 构造 ====================
TEST(Vector, DefaultConstruct) {
    vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
}

TEST(Vector, FillConstruct) {
    vector<int> v(5, 42);
    EXPECT_EQ(v.size(), 5u);
    for (auto x : v) EXPECT_EQ(x, 42);
}

TEST(Vector, InitList) {
    vector<int> v = {1, 2, 3, 4, 5};
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1); EXPECT_EQ(v[4], 5);
}

TEST(Vector, CopyConstruct) {
    vector<int> a = {1, 2, 3};
    vector<int> b = a;
    EXPECT_EQ(a, b);
    b[0] = 99;
    EXPECT_NE(a[0], 99);  // 深拷贝，独立
}

TEST(Vector, MoveConstruct) {
    vector<int> a = {1, 2, 3};
    vector<int> b = std::move(a);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_TRUE(a.empty());
}

// ==================== push_back & 扩容 ====================
TEST(Vector, PushBack_Growth) {
    vector<int> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 100u);
    for (int i = 0; i < 100; ++i) EXPECT_EQ(v[i], i);
    // 容量应 >= size
    EXPECT_GE(v.capacity(), 100u);
}

TEST(Vector, EmplaceBack) {
    vector<std::string> v;
    v.emplace_back(5, 'a');
    EXPECT_EQ(v.back(), "aaaaa");
}

TEST(Vector, PopBack) {
    vector<int> v = {1, 2, 3};
    v.pop_back();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.back(), 2);
}

// ==================== insert / erase ====================
TEST(Vector, InsertMiddle) {
    vector<int> v = {1, 2, 4, 5};
    v.insert(v.begin() + 2, 3);
    EXPECT_EQ(v.size(), 5u);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(v[i], i + 1);
}

TEST(Vector, EraseMiddle) {
    vector<int> v = {1, 2, 3, 4, 5};
    v.erase(v.begin() + 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[2], 4);
}

TEST(Vector, EraseRange) {
    vector<int> v = {1, 2, 3, 4, 5};
    v.erase(v.begin() + 1, v.begin() + 4);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1); EXPECT_EQ(v[1], 5);
}

// ==================== reverse_iterator ====================
TEST(Vector, ReverseIteration) {
    vector<int> v = {1, 2, 3, 4, 5};
    std::vector<int> reversed;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
        reversed.push_back(*it);
    EXPECT_EQ(reversed[0], 5);
    EXPECT_EQ(reversed[4], 1);
}

// ==================== resize / reserve ====================
TEST(Vector, Resize_Grow) {
    vector<int> v = {1, 2, 3};
    v.resize(6, 0);
    EXPECT_EQ(v.size(), 6u);
    EXPECT_EQ(v[5], 0);
}

TEST(Vector, Resize_Shrink) {
    vector<int> v = {1, 2, 3, 4, 5};
    v.resize(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v.back(), 3);
}

TEST(Vector, Reserve) {
    vector<int> v;
    v.reserve(100);
    EXPECT_GE(v.capacity(), 100u);
    EXPECT_EQ(v.size(), 0u);
}

// ==================== 元素访问 ====================
TEST(Vector, At_InBounds) {
    vector<int> v = {10, 20, 30};
    EXPECT_EQ(v.at(1), 20);
}

TEST(Vector, At_OutOfRange) {
    vector<int> v = {1, 2, 3};
    EXPECT_THROW(v.at(5), std::out_of_range);
}

// ==================== 赋值 ====================
TEST(Vector, CopyAssign) {
    vector<int> a = {1, 2, 3};
    vector<int> b;
    b = a;
    EXPECT_EQ(a, b);
}

TEST(Vector, MoveAssign) {
    vector<int> a = {1, 2, 3};
    vector<int> b;
    b = std::move(a);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_TRUE(a.empty());
}

// ==================== 非 trivial 类型 ====================
TEST(Vector, StringElements) {
    vector<std::string> v;
    v.push_back("hello");
    v.push_back("world");
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "hello");
    EXPECT_EQ(v[1], "world");
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
}
