#pragma once
// iterator_traits.h — 迭代器萃取机
//
// 核心问题：算法想要统一地问"这个迭代器的 value_type 是什么？"
// 但原生指针 (int*) 没有成员类型，没有 ::value_type。
//
// 解法：模板 + 偏特化
//   - 主模板：从类型自身的 nested typedef 读取
//   - 偏特化 T*：硬编码 value_type = T，pointer = T*，...
//   - 偏特化 const T*：同上，value_type 仍为 T（非 const）
//
// 有了 iterator_traits，算法可以用统一的语法：
//   typename iterator_traits<It>::value_type

#include "iterator_base.h"

namespace mystl {

// ---------- 主模板：适用于定义了 nested typedef 的迭代器类 ----------
template <typename Iterator>
struct iterator_traits {
    using iterator_category = typename Iterator::iterator_category;
    using value_type        = typename Iterator::value_type;
    using difference_type   = typename Iterator::difference_type;
    using pointer           = typename Iterator::pointer;
    using reference         = typename Iterator::reference;
};

// ---------- 偏特化：T* ----------
template <typename T>
struct iterator_traits<T*> {
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;
};

// ---------- 偏特化：const T* ----------
template <typename T>
struct iterator_traits<const T*> {
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;          // 注意：去掉 const，value_type 是 T 而非 const T
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;
};

// ---------- 辅助函数：快速提取分类标签 ----------
template <typename Iterator>
inline typename iterator_traits<Iterator>::iterator_category
iterator_category(const Iterator&) {
    return typename iterator_traits<Iterator>::iterator_category{};
}

// 获取 difference_type 的零值（用于 distance 的返回类型）
template <typename Iterator>
inline typename iterator_traits<Iterator>::difference_type*
difference_type(const Iterator&) {
    return nullptr;
}

// 获取 value_type 指针（用于算法内部的类型推断）
template <typename Iterator>
inline typename iterator_traits<Iterator>::value_type*
value_type(const Iterator&) {
    return nullptr;
}

} // namespace mystl
