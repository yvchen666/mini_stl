#pragma once
// iterator_algo.h — advance 和 distance 的多版本实现
//
// 这两个函数是 iterator_traits 价值的最直接体现：
// 同一个函数名，对不同类别的迭代器走不同的代码路径。
//
//   distance：
//     random_access  → O(1)：last - first
//     其他           → O(n)：逐步前进计数
//
//   advance：
//     random_access  → O(1)：it += n
//     bidirectional  → O(|n|)：可前可后
//     input/forward  → O(n)：只能前进

#include "iterator_traits.h"
#include <iterator>  // std::iterator_traits for STL iterators

namespace mystl {

// ==================== distance ====================

// input / forward / bidirectional：O(n) 逐步计数
// 接受所有非 random_access 的 tag（包括 std 和 mystl 的）
template <typename InputIt>
inline typename std::iterator_traits<InputIt>::difference_type
distance_aux(InputIt first, InputIt last, std::input_iterator_tag) {
    typename std::iterator_traits<InputIt>::difference_type n = 0;
    while (first != last) { ++first; ++n; }
    return n;
}

// random_access：O(1)
template <typename RandomIt>
inline typename std::iterator_traits<RandomIt>::difference_type
distance_aux(RandomIt first, RandomIt last, std::random_access_iterator_tag) {
    return last - first;
}

template <typename InputIt>
inline typename std::iterator_traits<InputIt>::difference_type
distance(InputIt first, InputIt last) {
    return distance_aux(first, last,
        typename std::iterator_traits<InputIt>::iterator_category{});
}

// ==================== advance ====================

template <typename InputIt, typename Distance>
inline void advance_aux(InputIt& it, Distance n, std::input_iterator_tag) {
    while (n--) ++it;
}

template <typename BidirIt, typename Distance>
inline void advance_aux(BidirIt& it, Distance n, std::bidirectional_iterator_tag) {
    if (n > 0) while (n--) ++it;
    else        while (n++) --it;
}

template <typename RandomIt, typename Distance>
inline void advance_aux(RandomIt& it, Distance n, std::random_access_iterator_tag) {
    it += n;
}

template <typename InputIt, typename Distance>
inline void advance(InputIt& it, Distance n) {
    advance_aux(it, n,
        typename std::iterator_traits<InputIt>::iterator_category{});
}

// next / prev（C++11 便利函数）
template <typename InputIt>
inline InputIt next(InputIt it,
    typename iterator_traits<InputIt>::difference_type n = 1) {
    mystl::advance(it, n);
    return it;
}

template <typename BidirIt>
inline BidirIt prev(BidirIt it,
    typename iterator_traits<BidirIt>::difference_type n = 1) {
    mystl::advance(it, -n);
    return it;
}

} // namespace mystl
