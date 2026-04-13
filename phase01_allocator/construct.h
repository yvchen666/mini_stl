#pragma once
// construct.h — 对象构造与析构原语
//
// 职责：在已分配的原始内存上构造对象，以及析构对象。
// 不负责申请/释放内存，只管生命周期。
//
// 核心函数：
//   construct(p, ...)  — placement new，调用构造函数
//   destroy(p)         — 调用单个对象的析构函数
//   destroy(first,last)— 批量析构，对 trivially destructible 类型走空路径

#include <new>          // placement new
#include <type_traits>  // std::is_trivially_destructible

namespace mystl {

// ---------- construct ----------

// 默认构造
template <typename T>
inline void construct(T* p) {
    ::new (static_cast<void*>(p)) T();
}

// 拷贝构造（从值初始化）
template <typename T, typename Value>
inline void construct(T* p, const Value& value) {
    ::new (static_cast<void*>(p)) T(value);
}

// 完美转发：支持 emplace 语义（C++11）
template <typename T, typename... Args>
inline void construct(T* p, Args&&... args) {
    ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}

// ---------- destroy ----------

// 单对象析构
template <typename T>
inline void destroy(T* p) {
    p->~T();
}

// 范围析构的两条路径，通过 tag dispatch 选择

// trivially destructible：析构函数什么都不做，跳过循环，零开销
template <typename ForwardIt>
inline void destroy_aux(ForwardIt, ForwardIt, std::true_type) {}

// non-trivially destructible：必须逐个调用析构函数
template <typename ForwardIt>
inline void destroy_aux(ForwardIt first, ForwardIt last, std::false_type) {
    for (; first != last; ++first) {
        destroy(&*first);
    }
}

// 范围析构：自动萃取 value_type 的析构特性
template <typename ForwardIt>
inline void destroy(ForwardIt first, ForwardIt last) {
    using value_type = typename std::iterator_traits<ForwardIt>::value_type;
    destroy_aux(first, last, std::is_trivially_destructible<value_type>{});
}

// 特化：原生指针 char* / wchar_t* 什么都不做
inline void destroy(char*, char*) {}
inline void destroy(wchar_t*, wchar_t*) {}

} // namespace mystl
