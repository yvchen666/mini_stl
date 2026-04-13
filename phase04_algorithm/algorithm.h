#pragma once
// algorithm.h — STL 核心算法（introsort + 其他高频算法）
//
// 实现的算法：
//   查找：    find, find_if
//   拷贝：    copy, fill, fill_n（对 trivially_copyable 类型 memmove 优化）
//   排序：    sort（introsort = 快排 + 堆排 + 插入排序）
//   归并：    merge, inplace_merge
//   二分：    lower_bound, upper_bound, binary_search
//   排列：    next_permutation, prev_permutation
//   其他：    min, max, swap, reverse

#include "../phase02_iterator/iterator.h"
#include "../phase03_containers/heap_priority_queue.h"
#include <cstring>       // memmove
#include <type_traits>
#include <functional>    // std::less
#include <utility>       // std::swap, std::move

namespace mystl {

// ============================================================
// 基础工具
// ============================================================

template <typename T>
const T& min(const T& a, const T& b) { return b < a ? b : a; }

template <typename T>
const T& max(const T& a, const T& b) { return a < b ? b : a; }

template <typename T>
void swap(T& a, T& b) noexcept {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}

template <typename ForwardIt>
void iter_swap(ForwardIt a, ForwardIt b) {
    mystl::swap(*a, *b);
}

// ============================================================
// find / find_if
// ============================================================

template <typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value) {
    while (first != last && !(*first == value)) ++first;
    return first;
}

template <typename InputIt, typename Pred>
InputIt find_if(InputIt first, InputIt last, Pred pred) {
    while (first != last && !pred(*first)) ++first;
    return first;
}

// ============================================================
// copy
// ============================================================

// trivially copyable：memmove
template <typename T>
T* copy_aux(const T* first, const T* last, T* result, std::true_type) {
    std::size_t n = static_cast<std::size_t>(last - first);
    std::memmove(result, first, n * sizeof(T));
    return result + n;
}

template <typename InputIt, typename OutputIt>
OutputIt copy_aux(InputIt first, InputIt last, OutputIt result, std::false_type) {
    while (first != last) *result++ = *first++;
    return result;
}

template <typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt result) {
    using T = typename std::iterator_traits<InputIt>::value_type;
    return copy_aux(first, last, result,
        std::is_trivially_copy_assignable<T>{});
}

// copy_backward
template <typename BidirIt1, typename BidirIt2>
BidirIt2 copy_backward(BidirIt1 first, BidirIt1 last, BidirIt2 result) {
    while (last != first) *--result = *--last;
    return result;
}

// ============================================================
// fill / fill_n
// ============================================================

template <typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value) {
    while (first != last) *first++ = value;
}

// 对 char/unsigned char 特化为 memset
inline void fill(char* first, char* last, char value) {
    std::memset(first, static_cast<unsigned char>(value),
                static_cast<std::size_t>(last - first));
}

template <typename OutputIt, typename Size, typename T>
OutputIt fill_n(OutputIt first, Size n, const T& value) {
    while (n-- > 0) *first++ = value;
    return first;
}

// ============================================================
// reverse
// ============================================================

template <typename BidirIt>
void reverse(BidirIt first, BidirIt last) {
    while (first != last && first != --last)
        iter_swap(first++, last);
}

// ============================================================
// sort — introsort（快排 + 堆排 + 插入排序）
// ============================================================
// 这是 std::sort 的真实算法：
//   - 快排（pivot 取三数中值）：O(n log n) 平均，O(n²) 最坏
//   - 当递归深度超过 2*log2(n) 时，切换到堆排序（保证最坏 O(n log n)）
//   - 当区间小于 16 时，用插入排序（小数组上常数因子最优）

static const std::ptrdiff_t INSERTION_SORT_THRESHOLD = 16;

// 插入排序（对小范围有序数据最优）
template <typename RandomIt, typename Compare>
void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
    if (first == last) return;
    for (auto i = first + 1; i != last; ++i) {
        auto value = std::move(*i);
        auto j = i;
        while (j != first && comp(value, *(j - 1))) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(value);
    }
}

// 三数中值（pivot 选择策略，减少最坏情况）
template <typename RandomIt, typename Compare>
RandomIt median_of_three(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
    if (comp(*a, *b)) {
        if (comp(*b, *c)) return b;
        if (comp(*a, *c)) return c;
        return a;
    }
    if (comp(*a, *c)) return a;
    if (comp(*b, *c)) return c;
    return b;
}

// 计算 introsort 的最大递归深度
inline std::ptrdiff_t introsort_depth(std::ptrdiff_t n) {
    std::ptrdiff_t depth = 0;
    while (n > 1) { n >>= 1; depth += 2; }
    return depth;
}

// 堆排序（当递归过深时使用）
template <typename RandomIt, typename Compare>
void heapsort(RandomIt first, RandomIt last, Compare comp) {
    mystl::make_heap(first, last, comp);
    mystl::sort_heap(first, last, comp);
}

// introsort 主体
template <typename RandomIt, typename Compare>
void introsort(RandomIt first, RandomIt last, std::ptrdiff_t depth_limit,
               Compare comp) {
    while (last - first > INSERTION_SORT_THRESHOLD) {
        if (depth_limit == 0) {
            heapsort(first, last, comp);
            return;
        }
        --depth_limit;
        // pivot：三数中值
        auto mid = first + (last - first) / 2;
        auto pivot = median_of_three(first, mid, last - 1, comp);
        iter_swap(pivot, last - 1);

        // 分区（Hoare 风格）
        auto p = first, q = last - 2;
        auto pivot_val = *(last - 1);
        while (true) {
            while (comp(*p, pivot_val)) ++p;
            while (p < q && !comp(*q, pivot_val)) --q;
            if (p >= q) break;
            iter_swap(p, q);
        }
        iter_swap(p, last - 1);

        // 递归处理较小的分区，循环处理较大的（尾递归优化）
        introsort(p + 1, last, depth_limit, comp);
        last = p;
    }
}

template <typename RandomIt, typename Compare>
void sort(RandomIt first, RandomIt last, Compare comp) {
    if (last - first < 2) return;
    std::ptrdiff_t depth = introsort_depth(last - first);
    introsort(first, last, depth, comp);
    insertion_sort(first, last, comp);  // 最终通扫，处理各分区之间的尾部
}

template <typename RandomIt>
void sort(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    mystl::sort(first, last, std::less<T>{});
}

// ============================================================
// merge / inplace_merge
// ============================================================

template <typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt merge(InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2,
               OutputIt result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first2, *first1)) *result++ = *first2++;
        else                        *result++ = *first1++;
    }
    return mystl::copy(first2, last2,
           mystl::copy(first1, last1, result));
}

template <typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt merge(InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2,
               OutputIt result) {
    using T = typename std::iterator_traits<InputIt1>::value_type;
    return mystl::merge(first1, last1, first2, last2, result, std::less<T>{});
}

// inplace_merge（原地，借临时缓冲区存放左半，然后归并回原数组）
template <typename BidirIt, typename Compare>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last, Compare comp) {
    using T = typename std::iterator_traits<BidirIt>::value_type;
    // 将左半段复制到临时缓冲区
    std::size_t n1 = static_cast<std::size_t>(mystl::distance(first, middle));
    std::size_t n2 = static_cast<std::size_t>(mystl::distance(middle, last));
    T* buf = new T[n1];
    // 把 [first, middle) 拷贝到 buf
    for (std::size_t i = 0; i < n1; ++i) buf[i] = std::move(first[i]);
    // 从 buf 和 [middle, last) 归并回 [first, last)
    T* lb = buf, *le = buf + n1;  // 左半：buf
    BidirIt ri = middle;           // 右半：仍在原数组
    BidirIt out = first;
    while (lb != le && ri != last) {
        if (comp(*ri, *lb)) *out++ = std::move(*ri++);
        else                *out++ = std::move(*lb++);
    }
    while (lb != le)   *out++ = std::move(*lb++);
    while (ri != last) *out++ = std::move(*ri++);
    delete[] buf;
}

template <typename BidirIt>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last) {
    using T = typename std::iterator_traits<BidirIt>::value_type;
    // 用 std::less 直接调有参版，显式 mystl:: 前缀避免 ADL
    ::mystl::inplace_merge(first, middle, last, std::less<T>{});
}

// ============================================================
// lower_bound / upper_bound / binary_search
// ============================================================

template <typename ForwardIt, typename T, typename Compare>
ForwardIt lower_bound(ForwardIt first, ForwardIt last,
                      const T& value, Compare comp) {
    auto n = mystl::distance(first, last);
    while (n > 0) {
        auto half = n / 2;
        auto mid = mystl::next(first, half);
        if (comp(*mid, value)) { first = mystl::next(mid); n -= half + 1; }
        else n = half;
    }
    return first;
}

template <typename ForwardIt, typename T>
ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value) {
    return mystl::lower_bound(first, last, value, std::less<T>{});
}

template <typename ForwardIt, typename T, typename Compare>
ForwardIt upper_bound(ForwardIt first, ForwardIt last,
                      const T& value, Compare comp) {
    auto n = mystl::distance(first, last);
    while (n > 0) {
        auto half = n / 2;
        auto mid = mystl::next(first, half);
        if (!comp(value, *mid)) { first = mystl::next(mid); n -= half + 1; }
        else n = half;
    }
    return first;
}

template <typename ForwardIt, typename T>
ForwardIt upper_bound(ForwardIt first, ForwardIt last, const T& value) {
    return mystl::upper_bound(first, last, value, std::less<T>{});
}

template <typename ForwardIt, typename T, typename Compare>
bool binary_search(ForwardIt first, ForwardIt last,
                   const T& value, Compare comp) {
    auto it = mystl::lower_bound(first, last, value, comp);
    return it != last && !comp(value, *it);
}

template <typename ForwardIt, typename T>
bool binary_search(ForwardIt first, ForwardIt last, const T& value) {
    return mystl::binary_search(first, last, value, std::less<T>{});
}

// ============================================================
// next_permutation / prev_permutation
// ============================================================

// next_permutation：将序列变为下一个排列，如已是最大排列则变为最小返回 false
template <typename BidirIt, typename Compare>
bool next_permutation(BidirIt first, BidirIt last, Compare comp) {
    if (first == last) return false;
    auto i = last;
    if (first == --i) return false;
    for (;;) {
        auto ip1 = i;
        if (comp(*--i, *ip1)) {
            // 找最右边比 *i 大的元素
            auto j = last;
            while (!comp(*i, *--j));
            iter_swap(i, j);
            mystl::reverse(ip1, last);
            return true;
        }
        if (i == first) {
            mystl::reverse(first, last);
            return false;
        }
    }
}

template <typename BidirIt>
bool next_permutation(BidirIt first, BidirIt last) {
    using T = typename std::iterator_traits<BidirIt>::value_type;
    return mystl::next_permutation(first, last, std::less<T>{});
}

template <typename BidirIt, typename Compare>
bool prev_permutation(BidirIt first, BidirIt last, Compare comp) {
    if (first == last) return false;
    auto i = last;
    if (first == --i) return false;
    for (;;) {
        auto ip1 = i;
        if (comp(*ip1, *--i)) {
            auto j = last;
            while (!comp(*--j, *i));
            iter_swap(i, j);
            mystl::reverse(ip1, last);
            return true;
        }
        if (i == first) {
            mystl::reverse(first, last);
            return false;
        }
    }
}

template <typename BidirIt>
bool prev_permutation(BidirIt first, BidirIt last) {
    using T = typename std::iterator_traits<BidirIt>::value_type;
    return mystl::prev_permutation(first, last, std::less<T>{});
}

// ============================================================
// count / count_if / equal / mismatch
// ============================================================

template <typename InputIt, typename T>
std::ptrdiff_t count(InputIt first, InputIt last, const T& value) {
    std::ptrdiff_t n = 0;
    while (first != last) { if (*first++ == value) ++n; }
    return n;
}

template <typename InputIt, typename Pred>
std::ptrdiff_t count_if(InputIt first, InputIt last, Pred pred) {
    std::ptrdiff_t n = 0;
    while (first != last) { if (pred(*first++)) ++n; }
    return n;
}

template <typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
    while (first1 != last1) if (!(*first1++ == *first2++)) return false;
    return true;
}

} // namespace mystl
