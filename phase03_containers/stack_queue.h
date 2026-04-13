#pragma once
// stack_queue.h — 基于 deque 的容器适配器
//
// stack  — LIFO，只暴露 top/push/pop
// queue  — FIFO，只暴露 front/back/push/pop
//
// 适配器模式的核心：通过组合复用 deque 的实现，
// 限制接口来提供不同的语义。

#include "deque.h"

namespace mystl {

// ==================== stack ====================
template <typename T, typename Container = mystl::deque<T>>
class stack {
public:
    using value_type      = T;
    using container_type  = Container;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = typename Container::size_type;

    stack() = default;
    explicit stack(const Container& c) : c_(c) {}
    explicit stack(Container&& c) : c_(std::move(c)) {}

    bool      empty() const { return c_.empty(); }
    size_type size()  const { return c_.size(); }

    reference       top()       { return c_.back(); }
    const_reference top() const { return c_.back(); }

    void push(const T& v) { c_.push_back(v); }
    void push(T&& v)      { c_.push_back(std::move(v)); }

    template <typename... Args>
    void emplace(Args&&... args) { c_.push_back(T(std::forward<Args>(args)...)); }

    void pop() { c_.pop_back(); }

    void swap(stack& other) noexcept { c_.swap(other.c_); }

    const Container& container() const { return c_; }

    template <typename U, typename C>
    friend bool operator==(const stack<U,C>&, const stack<U,C>&);

private:
    Container c_;
};

template <typename T, typename C>
bool operator==(const stack<T,C>& a, const stack<T,C>& b) {
    return a.c_ == b.c_;
}
template <typename T, typename C>
bool operator!=(const stack<T,C>& a, const stack<T,C>& b) { return !(a == b); }

// ==================== queue ====================
template <typename T, typename Container = mystl::deque<T>>
class queue {
public:
    using value_type      = T;
    using container_type  = Container;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = typename Container::size_type;

    queue() = default;
    explicit queue(const Container& c) : c_(c) {}
    explicit queue(Container&& c) : c_(std::move(c)) {}

    bool      empty() const { return c_.empty(); }
    size_type size()  const { return c_.size(); }

    reference       front()       { return c_.front(); }
    const_reference front() const { return c_.front(); }
    reference       back()        { return c_.back(); }
    const_reference back()  const { return c_.back(); }

    void push(const T& v) { c_.push_back(v); }
    void push(T&& v)      { c_.push_back(std::move(v)); }

    template <typename... Args>
    void emplace(Args&&... args) { c_.push_back(T(std::forward<Args>(args)...)); }

    void pop() { c_.pop_front(); }

    void swap(queue& other) noexcept { c_.swap(other.c_); }

    template <typename U, typename C>
    friend bool operator==(const queue<U,C>&, const queue<U,C>&);

private:
    Container c_;
};

template <typename T, typename C>
bool operator==(const queue<T,C>& a, const queue<T,C>& b) {
    return a.c_ == b.c_;
}
template <typename T, typename C>
bool operator!=(const queue<T,C>& a, const queue<T,C>& b) { return !(a == b); }

} // namespace mystl
