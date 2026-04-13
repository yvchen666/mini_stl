#include <gtest/gtest.h>
#include "iterator.h"

#include <list>
#include <vector>
#include <numeric>   // std::iota

// ============================================================
// iterator_traits
// ============================================================

TEST(IteratorTraits, RawPointer) {
    using Traits = mystl::iterator_traits<int*>;
    static_assert(std::is_same<Traits::value_type, int>::value, "");
    static_assert(std::is_same<Traits::pointer,    int*>::value, "");
    static_assert(std::is_same<Traits::reference,  int&>::value, "");
    static_assert(std::is_same<
        Traits::iterator_category,
        mystl::random_access_iterator_tag>::value, "");
    // 编译通过即测试通过
}

TEST(IteratorTraits, ConstPointer) {
    using Traits = mystl::iterator_traits<const double*>;
    // value_type 应去掉 const
    static_assert(std::is_same<Traits::value_type, double>::value, "");
    static_assert(std::is_same<Traits::pointer, const double*>::value, "");
}

// 自定义迭代器：继承 mystl::iterator 基类
struct MyInputIt : mystl::iterator<mystl::input_iterator_tag, int> {
    int val;
    explicit MyInputIt(int v) : val(v) {}
    bool operator==(const MyInputIt& o) const { return val == o.val; }
    bool operator!=(const MyInputIt& o) const { return val != o.val; }
    int  operator*() const { return val; }
    MyInputIt& operator++() { ++val; return *this; }
};

TEST(IteratorTraits, CustomIterator) {
    using Traits = mystl::iterator_traits<MyInputIt>;
    static_assert(std::is_same<Traits::value_type, int>::value, "");
    static_assert(std::is_same<
        Traits::iterator_category,
        mystl::input_iterator_tag>::value, "");
}

// ============================================================
// distance
// ============================================================

TEST(Distance, RandomAccessPointer) {
    int arr[5];
    auto d = mystl::distance(arr, arr + 5);
    EXPECT_EQ(d, 5);
}

TEST(Distance, InputIterator) {
    // 用自定义 input_iterator 验证 O(n) 路径
    MyInputIt first(0), last(5);
    auto d = mystl::distance(first, last);
    EXPECT_EQ(d, 5);
}

TEST(Distance, StdListIterator) {
    std::list<int> lst = {1, 2, 3, 4, 5};
    // std::list 的迭代器是 bidirectional，走 O(n) 路径
    auto d = mystl::distance(lst.begin(), lst.end());
    EXPECT_EQ(d, 5);
}

// ============================================================
// advance
// ============================================================

TEST(Advance, RandomAccess) {
    int arr[10];
    std::iota(arr, arr + 10, 0);
    int* p = arr;
    mystl::advance(p, 7);
    EXPECT_EQ(*p, 7);
}

TEST(Advance, Bidirectional_Forward) {
    std::list<int> lst = {10, 20, 30, 40, 50};
    auto it = lst.begin();
    mystl::advance(it, 3);
    EXPECT_EQ(*it, 40);
}

TEST(Advance, Bidirectional_Backward) {
    std::list<int> lst = {10, 20, 30, 40, 50};
    auto it = lst.end();
    mystl::advance(it, -2);
    EXPECT_EQ(*it, 40);
}

// next / prev
TEST(Advance, NextPrev) {
    int arr[5] = {0, 1, 2, 3, 4};
    int* p = arr;
    EXPECT_EQ(*mystl::next(p, 3), 3);
    EXPECT_EQ(*mystl::prev(p + 4, 2), 2);
}

// ============================================================
// reverse_iterator
// ============================================================

TEST(ReverseIterator, BasicTraversal) {
    int arr[5] = {1, 2, 3, 4, 5};

    using RevIt = mystl::reverse_iterator<int*>;
    RevIt rbegin(arr + 5);
    RevIt rend(arr);

    // rbegin 应指向最后一个元素（5），rend 到 1
    std::vector<int> result;
    for (auto it = rbegin; it != rend; ++it) {
        result.push_back(*it);
    }
    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[4], 1);
}

TEST(ReverseIterator, Deref_IsOneBefore_Base) {
    // 关键：*reverse_iterator(p) == *(p-1)
    int arr[3] = {10, 20, 30};
    mystl::reverse_iterator<int*> rev(arr + 3);  // base = end
    EXPECT_EQ(*rev, 30);  // *(end-1) = arr[2] = 30
    ++rev;
    EXPECT_EQ(*rev, 20);  // *(end-2) = arr[1] = 20
}

TEST(ReverseIterator, RandomAccessOps) {
    int arr[5] = {0, 1, 2, 3, 4};
    mystl::reverse_iterator<int*> rbegin(arr + 5);

    EXPECT_EQ(rbegin[0], 4);   // *(end-1)
    EXPECT_EQ(rbegin[1], 3);   // *(end-2)
    EXPECT_EQ(rbegin[4], 0);   // *(end-5) = arr[0]

    auto it = rbegin + 2;
    EXPECT_EQ(*it, 2);

    EXPECT_EQ(mystl::distance(rbegin, rbegin + 5), 5);
}

TEST(ReverseIterator, Comparison) {
    int arr[5] = {1, 2, 3, 4, 5};
    mystl::reverse_iterator<int*> r1(arr + 5);
    mystl::reverse_iterator<int*> r2(arr + 3);

    EXPECT_TRUE(r1 < r2);   // r1 在逻辑上更靠前（base 更大 → 逻辑位置更小）
    EXPECT_TRUE(r2 > r1);
    EXPECT_FALSE(r1 == r2);

    auto diff = r2 - r1;
    EXPECT_EQ(diff, 2);
}

TEST(ReverseIterator, BaseProperty) {
    int arr[5] = {1, 2, 3, 4, 5};
    mystl::reverse_iterator<int*> rev(arr + 3);  // base = arr+3, 逻辑上指向 arr[2]=3
    EXPECT_EQ(*rev, 3);
    EXPECT_EQ(rev.base(), arr + 3);  // base 比实际指向的位置前进一步
}
