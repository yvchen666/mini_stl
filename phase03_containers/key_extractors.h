#pragma once
// key_extractors.h — 键提取仿函数（供 rb_tree、hashtable 等使用）

namespace mystl {

// value 就是 key（用于 set）
template <typename T>
struct Identity {
    const T& operator()(const T& v) const { return v; }
};

// pair 的 first 是 key（用于 map）
template <typename Pair>
struct SelectFirst {
    const typename Pair::first_type& operator()(const Pair& p) const {
        return p.first;
    }
};

} // namespace mystl
