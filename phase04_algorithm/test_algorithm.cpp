#include <gtest/gtest.h>
#include "algorithm.h"
#include "../phase03_containers/vector.h"
#include <string>
#include <algorithm>
#include <numeric>

// ============================================================
// find / find_if
// ============================================================
TEST(Find, BasicFind) {
    int arr[] = {1, 2, 3, 4, 5};
    auto it = mystl::find(arr, arr + 5, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(it, arr + 2);
}

TEST(Find, NotFound) {
    int arr[] = {1, 2, 3};
    auto it = mystl::find(arr, arr + 3, 99);
    EXPECT_EQ(it, arr + 3);
}

TEST(Find, FindIf) {
    int arr[] = {1, 3, 5, 6, 7};
    auto it = mystl::find_if(arr, arr + 5, [](int x){ return x % 2 == 0; });
    EXPECT_EQ(*it, 6);
}

// ============================================================
// copy / fill
// ============================================================
TEST(Copy, Trivial) {
    int src[] = {1, 2, 3, 4, 5};
    int dst[5] = {};
    mystl::copy(src, src + 5, dst);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(dst[i], src[i]);
}

TEST(Copy, NonTrivial) {
    std::string src[] = {"a", "bb", "ccc"};
    std::string dst[3];
    mystl::copy(src, src + 3, dst);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(dst[i], src[i]);
}

TEST(Fill, BasicFill) {
    int arr[5];
    mystl::fill(arr, arr + 5, 42);
    for (auto v : arr) EXPECT_EQ(v, 42);
}

TEST(Fill, FillN) {
    int arr[10] = {};
    mystl::fill_n(arr, 5, 7);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(arr[i], 7);
    for (int i = 5; i < 10; ++i) EXPECT_EQ(arr[i], 0);
}

// ============================================================
// reverse
// ============================================================
TEST(Reverse, Array) {
    int arr[] = {1, 2, 3, 4, 5};
    mystl::reverse(arr, arr + 5);
    int expected[] = {5, 4, 3, 2, 1};
    for (int i = 0; i < 5; ++i) EXPECT_EQ(arr[i], expected[i]);
}

// ============================================================
// sort（introsort）
// ============================================================
TEST(Sort, RandomOrder) {
    mystl::vector<int> v = {5, 3, 8, 1, 7, 2, 4, 6};
    mystl::sort(v.begin(), v.end());
    for (std::size_t i = 1; i < v.size(); ++i) EXPECT_LE(v[i-1], v[i]);
}

TEST(Sort, AlreadySorted) {
    int arr[] = {1, 2, 3, 4, 5};
    mystl::sort(arr, arr + 5);
    for (int i = 0; i < 4; ++i) EXPECT_LE(arr[i], arr[i+1]);
}

TEST(Sort, ReverseSorted) {
    int arr[] = {5, 4, 3, 2, 1};
    mystl::sort(arr, arr + 5);
    for (int i = 0; i < 4; ++i) EXPECT_LE(arr[i], arr[i+1]);
}

TEST(Sort, WithDuplicates) {
    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
    mystl::sort(arr, arr + 11);
    for (int i = 0; i < 10; ++i) EXPECT_LE(arr[i], arr[i+1]);
}

TEST(Sort, Descending_WithGreater) {
    int arr[] = {3, 1, 4, 1, 5, 9};
    mystl::sort(arr, arr + 6, std::greater<int>{});
    for (int i = 0; i < 5; ++i) EXPECT_GE(arr[i], arr[i+1]);
}

TEST(Sort, Large) {
    mystl::vector<int> v;
    for (int i = 1000; i >= 1; --i) v.push_back(i);
    mystl::sort(v.begin(), v.end());
    for (std::size_t i = 1; i < v.size(); ++i) EXPECT_LT(v[i-1], v[i]);
}

// ============================================================
// merge / inplace_merge
// ============================================================
TEST(Merge, TwoSortedArrays) {
    int a[] = {1, 3, 5, 7};
    int b[] = {2, 4, 6, 8};
    int c[8];
    mystl::merge(a, a+4, b, b+4, c);
    for (int i = 0; i < 7; ++i) EXPECT_LT(c[i], c[i+1]);
}

TEST(Merge, InplaceMerge) {
    int arr[] = {1, 3, 5, 2, 4, 6};
    // [1,3,5] 和 [2,4,6] 各自有序
    mystl::inplace_merge(arr, arr + 3, arr + 6);
    for (int i = 0; i < 5; ++i) EXPECT_LE(arr[i], arr[i+1]);
}

// ============================================================
// lower_bound / upper_bound / binary_search
// ============================================================
TEST(BinarySearch, LowerBound) {
    int arr[] = {1, 3, 5, 7, 9};
    auto it = mystl::lower_bound(arr, arr + 5, 4);
    EXPECT_EQ(*it, 5);  // 第一个 >= 4 的是 5
    it = mystl::lower_bound(arr, arr + 5, 5);
    EXPECT_EQ(*it, 5);  // 精确匹配
}

TEST(BinarySearch, UpperBound) {
    int arr[] = {1, 3, 5, 5, 5, 7};
    auto it = mystl::upper_bound(arr, arr + 6, 5);
    EXPECT_EQ(*it, 7);  // 第一个 > 5 的是 7
}

TEST(BinarySearch, BinarySearch) {
    int arr[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mystl::binary_search(arr, arr + 5, 3));
    EXPECT_FALSE(mystl::binary_search(arr, arr + 5, 6));
}

// ============================================================
// next_permutation / prev_permutation
// ============================================================
TEST(Permutation, NextPermutation) {
    int arr[] = {1, 2, 3};
    // 全排列：共 6 个
    int count = 1;
    while (mystl::next_permutation(arr, arr + 3)) ++count;
    EXPECT_EQ(count, 6);
    // 循环后应恢复到最小排列 {1,2,3}
    EXPECT_EQ(arr[0], 1); EXPECT_EQ(arr[1], 2); EXPECT_EQ(arr[2], 3);
}

TEST(Permutation, NextPermutation_Specific) {
    int arr[] = {1, 2, 3};
    mystl::next_permutation(arr, arr + 3);
    EXPECT_EQ(arr[0], 1); EXPECT_EQ(arr[1], 3); EXPECT_EQ(arr[2], 2);
}

TEST(Permutation, PrevPermutation) {
    int arr[] = {3, 2, 1};
    int count = 1;
    while (mystl::prev_permutation(arr, arr + 3)) ++count;
    EXPECT_EQ(count, 6);
}

// ============================================================
// count / count_if / equal
// ============================================================
TEST(Other, Count) {
    int arr[] = {1, 2, 3, 2, 1, 2};
    EXPECT_EQ(mystl::count(arr, arr + 6, 2), 3);
}

TEST(Other, CountIf) {
    int arr[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(mystl::count_if(arr, arr + 5, [](int x){ return x % 2 == 0; }), 2);
}

TEST(Other, Equal) {
    int a[] = {1, 2, 3};
    int b[] = {1, 2, 3};
    int c[] = {1, 2, 4};
    EXPECT_TRUE(mystl::equal(a, a + 3, b));
    EXPECT_FALSE(mystl::equal(a, a + 3, c));
}
