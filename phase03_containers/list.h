#pragma once
// list.h — 带哨兵节点的环状双向链表
//
// 设计特点：
//   - 一个哨兵节点 (sentinel_)：head.prev = tail, tail.next = head
//     begin() = sentinel_.next, end() = &sentinel_
//     这样空链表的 begin() == end()，无需特殊处理
//   - 插入/删除不使迭代器失效（只改指针）
//   - 所有操作均不搬移数据，splice 是 O(1)

#include "../phase01_allocator/alloc.h"
#include "../phase01_allocator/construct.h"
#include "../phase02_iterator/iterator.h"
#include <algorithm>  // std::swap

namespace mystl {

// ---------- 链表节点 ----------
template <typename T>
struct list_node {
    list_node* prev;
    list_node* next;
    T          data;
};

// ---------- list 迭代器 ----------
template <typename T, typename Ref, typename Ptr>
struct list_iterator
    : iterator<bidirectional_iterator_tag, T,
               std::ptrdiff_t, Ptr, Ref>
{
    using self     = list_iterator<T, Ref, Ptr>;
    using node_ptr = list_node<T>*;

    node_ptr node_;

    list_iterator() : node_(nullptr) {}
    explicit list_iterator(node_ptr n) : node_(n) {}

    // 允许从 non-const 转换为 const
    list_iterator(const list_iterator<T, T&, T*>& other)
        : node_(other.node_) {}

    bool operator==(const self& o) const { return node_ == o.node_; }
    bool operator!=(const self& o) const { return node_ != o.node_; }

    Ref operator*()  const { return node_->data; }
    Ptr operator->() const { return &node_->data; }

    self& operator++()    { node_ = node_->next; return *this; }
    self  operator++(int) { auto t = *this; ++*this; return t; }
    self& operator--()    { node_ = node_->prev; return *this; }
    self  operator--(int) { auto t = *this; --*this; return t; }
};

// ---------- list 容器 ----------
template <typename T,
          typename NodeAlloc = Allocator<list_node<T>>>
class list {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = list_iterator<T, T&, T*>;
    using const_iterator  = list_iterator<T, const T&, const T*>;
    using reverse_iterator       = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // ==================== 构造 / 析构 ====================

    list() { init_sentinel(); }

    explicit list(size_type n) { init_sentinel(); for (size_type i = 0; i < n; ++i) emplace_back(T{}); }
    list(size_type n, const T& v) { init_sentinel(); for (size_type i = 0; i < n; ++i) push_back(v); }

    list(std::initializer_list<T> il) {
        init_sentinel();
        for (const auto& v : il) push_back(v);
    }

    list(const list& other) {
        init_sentinel();
        for (const auto& v : other) push_back(v);
    }

    list(list&& other) noexcept { steal(other); }

    ~list() {
        clear();
        free_node(sentinel_);
    }

    // ==================== 赋值 ====================

    list& operator=(const list& other) {
        if (this != &other) { clear(); for (const auto& v : other) push_back(v); }
        return *this;
    }
    list& operator=(list&& other) noexcept {
        if (this != &other) { clear(); free_node(sentinel_); steal(other); }
        return *this;
    }

    // ==================== 迭代器 ====================

    iterator       begin()  noexcept { return iterator(sentinel_->next); }
    const_iterator begin()  const noexcept { return const_iterator(sentinel_->next); }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator       end()    noexcept { return iterator(sentinel_); }
    const_iterator end()    const noexcept { return const_iterator(sentinel_); }
    const_iterator cend()   const noexcept { return end(); }

    reverse_iterator       rbegin()  noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator       rend()    noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }

    // ==================== 容量 ====================

    bool      empty()  const noexcept { return sentinel_->next == sentinel_; }
    size_type size()   const noexcept {
        size_type n = 0;
        for (auto it = begin(); it != end(); ++it) ++n;
        return n;
    }

    // ==================== 元素访问 ====================

    reference       front()       { return sentinel_->next->data; }
    const_reference front() const { return sentinel_->next->data; }
    reference       back()        { return sentinel_->prev->data; }
    const_reference back()  const { return sentinel_->prev->data; }

    // ==================== 修改操作 ====================

    void push_front(const T& v) { insert(begin(), v); }
    void push_front(T&& v)      { insert(begin(), std::move(v)); }
    void push_back(const T& v)  { insert(end(), v); }
    void push_back(T&& v)       { insert(end(), std::move(v)); }

    template <typename... Args>
    void emplace_front(Args&&... args) { emplace(begin(), std::forward<Args>(args)...); }
    template <typename... Args>
    void emplace_back(Args&&... args)  { emplace(end(), std::forward<Args>(args)...); }

    void pop_front() { erase(begin()); }
    void pop_back()  { erase(iterator(sentinel_->prev)); }

    // 在 pos 之前插入
    iterator insert(const_iterator pos, const T& v) {
        return emplace(pos, v);
    }
    iterator insert(const_iterator pos, T&& v) {
        return emplace(pos, std::move(v));
    }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        list_node<T>* nd = alloc_.allocate(1);
        construct(&nd->data, std::forward<Args>(args)...);
        list_node<T>* cur = pos.node_;
        nd->next = cur;
        nd->prev = cur->prev;
        cur->prev->next = nd;
        cur->prev = nd;
        return iterator(nd);
    }

    iterator erase(const_iterator pos) {
        list_node<T>* nd   = pos.node_;
        list_node<T>* next = nd->next;
        nd->prev->next = nd->next;
        nd->next->prev = nd->prev;
        destroy(&nd->data);
        free_node(nd);
        return iterator(next);
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) first = erase(first);
        return iterator(last.node_);
    }

    void clear() noexcept { erase(begin(), end()); }

    void resize(size_type n) { resize(n, T{}); }
    void resize(size_type n, const T& v) {
        size_type cur = size();
        if (n < cur) {
            auto it = begin();
            mystl::advance(it, static_cast<std::ptrdiff_t>(n));
            erase(it, end());
        } else {
            while (cur++ < n) push_back(v);
        }
    }

    // splice：把 [first,last) 从 other 移到 this 的 pos 之前
    void splice(const_iterator pos, list& other,
                const_iterator first, const_iterator last) {
        if (first == last) return;
        list_node<T>* f = first.node_;
        list_node<T>* l = last.node_->prev;
        // 从 other 摘出
        f->prev->next = last.node_;
        last.node_->prev = f->prev;
        // 插入到 this 的 pos 之前
        list_node<T>* p = pos.node_;
        f->prev = p->prev;
        l->next = p;
        p->prev->next = f;
        p->prev = l;
    }

    void splice(const_iterator pos, list& other) {
        splice(pos, other, other.begin(), other.end());
    }

    void splice(const_iterator pos, list& other, const_iterator it) {
        const_iterator nxt = it; ++nxt;
        splice(pos, other, it, nxt);
    }

    // remove：删除值等于 value 的所有节点
    void remove(const T& value) {
        for (auto it = begin(); it != end(); ) {
            if (*it == value) it = erase(it);
            else              ++it;
        }
    }

    // unique：删除连续重复元素
    void unique() {
        if (empty()) return;
        for (auto it = begin(), next = mystl::next(it); next != end(); ) {
            if (*it == *next) next = erase(next);
            else { it = next; ++next; }
        }
    }

    // sort：链表原地归并排序（不移动数据，只改指针）
    void sort() {
        if (empty() || sentinel_->next->next == sentinel_) return;
        // 把链表从中间断开，递归排序，然后归并
        list left, right;
        size_type mid = size() / 2;
        auto it = begin();
        mystl::advance(it, static_cast<std::ptrdiff_t>(mid));
        right.splice(right.begin(), *this, it, end());
        left.splice(left.begin(), *this, begin(), end());
        left.sort();
        right.sort();
        merge(left, right);
    }

    void swap(list& other) noexcept {
        std::swap(sentinel_, other.sentinel_);
    }

private:
    list_node<T>* sentinel_;
    NodeAlloc     alloc_;

    void init_sentinel() {
        sentinel_ = alloc_.allocate(1);
        sentinel_->prev = sentinel_;
        sentinel_->next = sentinel_;
    }

    void free_node(list_node<T>* nd) {
        alloc_.deallocate(nd, 1);
    }

    void steal(list& other) noexcept {
        sentinel_ = other.sentinel_;
        list_node<T>* dummy = alloc_.allocate(1);
        dummy->prev = dummy;
        dummy->next = dummy;
        other.sentinel_ = dummy;
    }

    // 归并 left 和 right 两个有序链表到 *this（*this 必须为空）
    void merge(list& left, list& right) {
        auto li = left.begin(), ri = right.begin();
        while (li != left.end() && ri != right.end()) {
            if (*li <= *ri) { auto nx = li; ++nx; splice(end(), left, li, nx); li = nx; }
            else            { auto nx = ri; ++nx; splice(end(), right, ri, nx); ri = nx; }
        }
        if (li != left.end())  splice(end(), left,  li, left.end());
        if (ri != right.end()) splice(end(), right, ri, right.end());
    }
};

template <typename T, typename A>
bool operator==(const list<T,A>& a, const list<T,A>& b) {
    auto ia = a.begin(), ib = b.begin();
    for (; ia != a.end() && ib != b.end(); ++ia, ++ib)
        if (*ia != *ib) return false;
    return ia == a.end() && ib == b.end();
}
template <typename T, typename A>
bool operator!=(const list<T,A>& a, const list<T,A>& b) { return !(a == b); }

} // namespace mystl
