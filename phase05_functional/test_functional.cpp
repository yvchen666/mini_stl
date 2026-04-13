#include <gtest/gtest.h>
#include "functional.h"
#include "../phase04_algorithm/algorithm.h"
#include "../phase03_containers/vector.h"
#include <string>

// ============================================================
// 基本算术仿函数
// ============================================================
TEST(Arithmetic, Plus) {
    mystl::plus<int> f;
    EXPECT_EQ(f(3, 4), 7);
    EXPECT_EQ(f(0, -1), -1);
}

TEST(Arithmetic, Minus) {
    mystl::minus<int> f;
    EXPECT_EQ(f(10, 3), 7);
}

TEST(Arithmetic, Multiplies) {
    mystl::multiplies<double> f;
    EXPECT_DOUBLE_EQ(f(2.5, 4.0), 10.0);
}

TEST(Arithmetic, Divides) {
    mystl::divides<int> f;
    EXPECT_EQ(f(10, 3), 3);  // 整数除法
}

TEST(Arithmetic, Modulus) {
    mystl::modulus<int> f;
    EXPECT_EQ(f(10, 3), 1);
}

TEST(Arithmetic, Negate) {
    mystl::negate<int> f;
    EXPECT_EQ(f(5), -5);
    EXPECT_EQ(f(-3), 3);
}

// ============================================================
// 比较仿函数
// ============================================================
TEST(Compare, Less_UsedAsComparator) {
    mystl::vector<int> v = {5, 3, 8, 1, 7};
    mystl::sort(v.begin(), v.end(), mystl::less<int>{});
    for (std::size_t i = 1; i < v.size(); ++i)
        EXPECT_LT(v[i-1], v[i]);
}

TEST(Compare, Greater_UsedAsComparator) {
    mystl::vector<int> v = {5, 3, 8, 1, 7};
    mystl::sort(v.begin(), v.end(), mystl::greater<int>{});
    for (std::size_t i = 1; i < v.size(); ++i)
        EXPECT_GT(v[i-1], v[i]);
}

TEST(Compare, EqualTo) {
    mystl::equal_to<std::string> eq;
    EXPECT_TRUE(eq("hello", "hello"));
    EXPECT_FALSE(eq("hello", "world"));
}

TEST(Compare, LessEqual) {
    mystl::less_equal<int> le;
    EXPECT_TRUE(le(3, 5));
    EXPECT_TRUE(le(5, 5));
    EXPECT_FALSE(le(6, 5));
}

// ============================================================
// 逻辑仿函数
// ============================================================
TEST(Logic, LogicalAnd) {
    mystl::logical_and<bool> f;
    EXPECT_TRUE(f(true, true));
    EXPECT_FALSE(f(true, false));
}

TEST(Logic, LogicalNot) {
    mystl::logical_not<bool> f;
    EXPECT_TRUE(f(false));
    EXPECT_FALSE(f(true));
}

// ============================================================
// not1 / not2
// ============================================================
TEST(Adapter, Not1) {
    // 找所有不是偶数的元素
    struct is_even : mystl::unary_function<int, bool> {
        bool operator()(int x) const { return x % 2 == 0; }
    };
    int arr[] = {1, 2, 3, 4, 5};
    auto it = mystl::find_if(arr, arr + 5, mystl::not1(is_even{}));
    EXPECT_EQ(*it, 1);  // 第一个奇数
}

TEST(Adapter, Not2) {
    // not2(less<int>{}) == greater_equal<int>{}
    auto not_less = mystl::not2(mystl::less<int>{});
    EXPECT_TRUE(not_less(5, 3));   // not(5 < 3) = not(false) = true
    EXPECT_TRUE(not_less(3, 3));   // not(3 < 3) = not(false) = true
    EXPECT_FALSE(not_less(2, 5));  // not(2 < 5) = not(true) = false
}

// ============================================================
// bind1st / bind2nd
// ============================================================
TEST(Adapter, Bind2nd_FindIfGreaterThan5) {
    // 等价于 C++11 的 [](int x){ return x > 5; }
    int arr[] = {1, 3, 5, 7, 9};
    auto gt5 = mystl::bind2nd(mystl::greater<int>{}, 5);
    auto it = mystl::find_if(arr, arr + 5, gt5);
    EXPECT_EQ(*it, 7);
}

TEST(Adapter, Bind1st_Multiply) {
    // bind1st(multiplies<int>{}, 3) → 3 * x
    auto triple = mystl::bind1st(mystl::multiplies<int>{}, 3);
    EXPECT_EQ(triple(4), 12);
    EXPECT_EQ(triple(7), 21);
}

TEST(Adapter, Bind2nd_CountGreaterThan10) {
    int arr[] = {5, 10, 15, 20, 3};
    auto gt10 = mystl::bind2nd(mystl::greater<int>{}, 10);
    auto n = mystl::count_if(arr, arr + 5, gt10);
    EXPECT_EQ(n, 2);  // 15 和 20
}

// ============================================================
// ptr_fun
// ============================================================
static bool is_positive(int x) { return x > 0; }
static int add_ten(int x) { return x + 10; }

TEST(Adapter, PtrFun_Unary) {
    auto pred = mystl::ptr_fun(is_positive);
    int arr[] = {-1, 0, 1, 2, -3};
    auto it = mystl::find_if(arr, arr + 5, pred);
    EXPECT_EQ(*it, 1);  // 第一个正数
}

TEST(Adapter, PtrFun_WithNot1) {
    // ptr_fun 把函数包装成有 argument_type 的仿函数，not1 才能接受
    auto not_positive = mystl::not1(mystl::ptr_fun(is_positive));
    int arr[] = {1, -2, 3, -4};
    auto it = mystl::find_if(arr, arr + 4, not_positive);
    EXPECT_EQ(*it, -2);
}

// ============================================================
// 与算法配合使用的综合测试
// ============================================================
TEST(Integration, SortDescending) {
    mystl::vector<int> v = {5, 1, 8, 3, 7};
    mystl::sort(v.begin(), v.end(), mystl::greater<int>{});
    EXPECT_EQ(v[0], 8);
    EXPECT_EQ(v[4], 1);
}

TEST(Integration, FindAll_EqualTo3) {
    int arr[] = {1, 3, 2, 3, 3, 4};
    int count = 0;
    int* p = arr;
    while ((p = mystl::find_if(p, arr + 6,
                mystl::bind2nd(mystl::equal_to<int>{}, 3))) != arr + 6) {
        ++count; ++p;
    }
    EXPECT_EQ(count, 3);
}
