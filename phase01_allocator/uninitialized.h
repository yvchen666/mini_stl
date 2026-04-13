#pragma once
// uninitialized.h — 在原始内存上批量构造对象
//
// 三个函数对应 std 标准库里的同名函数：
//   uninitialized_copy  — 从 [first, last) 拷贝构造到 [result, ...)
//   uninitialized_fill  — 用 value 填充构造 [first, last)
//   uninitialized_fill_n— 用 value 构造 n 个元素，从 first 开始
//
// 优化：对 trivially copyable 类型，直接用 memmove/memset，绕过逐个构造

#include <cstring>      // memmove
#include <type_traits>  // is_trivially_copy_assignable
#include "construct.h"

namespace mystl {

// ==================== uninitialized_copy ====================

template <typename InputIt, typename ForwardIt>
ForwardIt uninit_copy_aux(InputIt first, InputIt last, ForwardIt result,
                          std::true_type /*trivially copyable*/) {
    // 直接用 memmove（底层优化，允许重叠）
    std::size_t n = static_cast<std::size_t>(last - first);
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    std::memmove(static_cast<void*>(&*result),
                 static_cast<const void*>(&*first),
                 n * sizeof(T));
    return result + static_cast<std::ptrdiff_t>(n);
}

template <typename InputIt, typename ForwardIt>
ForwardIt uninit_copy_aux(InputIt first, InputIt last, ForwardIt result,
                          std::false_type /*non-trivial*/) {
    ForwardIt cur = result;
    try {
        for (; first != last; ++first, ++cur) {
            construct(&*cur, *first);
        }
    } catch (...) {
        destroy(result, cur);  // 异常安全：析构已构造的部分
        throw;
    }
    return cur;
}

template <typename InputIt, typename ForwardIt>
ForwardIt uninitialized_copy(InputIt first, InputIt last, ForwardIt result) {
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    return uninit_copy_aux(first, last, result,
        std::is_trivially_copy_assignable<T>{});
}

// ==================== uninitialized_fill ====================

template <typename ForwardIt, typename T>
void uninit_fill_aux(ForwardIt first, ForwardIt last, const T& value,
                     std::true_type /*trivial*/) {
    // 标准库 fill 对基本类型已经高度优化，直接用
    std::fill(first, last, value);
}

template <typename ForwardIt, typename T>
void uninit_fill_aux(ForwardIt first, ForwardIt last, const T& value,
                     std::false_type) {
    ForwardIt cur = first;
    try {
        for (; cur != last; ++cur) {
            construct(&*cur, value);
        }
    } catch (...) {
        destroy(first, cur);
        throw;
    }
}

template <typename ForwardIt, typename T>
void uninitialized_fill(ForwardIt first, ForwardIt last, const T& value) {
    using VT = typename std::iterator_traits<ForwardIt>::value_type;
    uninit_fill_aux(first, last, value, std::is_trivially_copy_assignable<VT>{});
}

// ==================== uninitialized_fill_n ====================

template <typename ForwardIt, typename Size, typename T>
ForwardIt uninit_fill_n_aux(ForwardIt first, Size n, const T& value,
                             std::true_type) {
    return std::fill_n(first, n, value);
}

template <typename ForwardIt, typename Size, typename T>
ForwardIt uninit_fill_n_aux(ForwardIt first, Size n, const T& value,
                             std::false_type) {
    ForwardIt cur = first;
    try {
        for (; n > 0; --n, ++cur) {
            construct(&*cur, value);
        }
    } catch (...) {
        destroy(first, cur);
        throw;
    }
    return cur;
}

template <typename ForwardIt, typename Size, typename T>
ForwardIt uninitialized_fill_n(ForwardIt first, Size n, const T& value) {
    using VT = typename std::iterator_traits<ForwardIt>::value_type;
    return uninit_fill_n_aux(first, n, value,
        std::is_trivially_copy_assignable<VT>{});
}

} // namespace mystl
