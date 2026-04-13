#pragma once
// iterator_base.h — 迭代器分类标签与基类模板
//
// STL 把迭代器分为 5 个级别，形成继承关系：
//
//   input_iterator_tag
//     └─ forward_iterator_tag
//           └─ bidirectional_iterator_tag
//                 └─ random_access_iterator_tag
//
//   output_iterator_tag（独立，不参与上面的继承链）
//
// 这套继承关系让算法能用 tag dispatch 自动选择最优实现：
// 对 random_access_iterator 的 advance 是 O(1)，
// 对 input_iterator 的 advance 是 O(n)。

#include <cstddef>  // ptrdiff_t, size_t
#include <iterator> // std 的迭代器 tag（作为基类）

namespace mystl {

// ---------- 迭代器类型标签 ----------
// 继承自 std 的同名 tag，双向兼容：
//   - mystl 算法（接受 std tag 参数）能处理 STL 迭代器
//   - std 算法能处理 mystl 迭代器

struct input_iterator_tag        : std::input_iterator_tag {};
struct output_iterator_tag       : std::output_iterator_tag {};
struct forward_iterator_tag      : std::forward_iterator_tag, input_iterator_tag {};
struct bidirectional_iterator_tag: std::bidirectional_iterator_tag, forward_iterator_tag {};
struct random_access_iterator_tag: std::random_access_iterator_tag, bidirectional_iterator_tag {};

// ---------- 迭代器基类模板 ----------
// 自定义迭代器继承此类，自动获得标准成员类型
template <
    typename Category,
    typename T,
    typename Distance  = std::ptrdiff_t,
    typename Pointer   = T*,
    typename Reference = T&>
struct iterator {
    using iterator_category = Category;
    using value_type        = T;
    using difference_type   = Distance;
    using pointer           = Pointer;
    using reference         = Reference;
};

} // namespace mystl
