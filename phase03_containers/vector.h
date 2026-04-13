#pragma once
// vector.h — 动态数组容器
//
// 内部结构：三根指针
//   start_          → 数组起始
//   finish_         → 已使用部分末尾（下一个空位）
//   end_of_storage_ → 分配内存末尾
//
// [start_) [start_, finish_) = 有效元素
// [finish_, end_of_storage_) = 预留空间（容量但未使用）
//
// 扩容策略（push_back 触发）：
//   新容量 = max(当前容量 * 2, 1)
//   旧元素拷贝到新内存 → 析构旧元素 → 释放旧内存

#include "../phase01_allocator/alloc.h"
#include "../phase01_allocator/construct.h"
#include "../phase01_allocator/uninitialized.h"
#include "../phase02_iterator/iterator.h"
#include <stdexcept>   // std::out_of_range
#include <algorithm>   // std::max

namespace mystl {

template <typename T, typename Alloc = Allocator<T>>
class vector {
public:
    // 标准成员类型
    using value_type             = T;
    using allocator_type         = Alloc;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = T&;
    using const_reference        = const T&;
    using pointer                = T*;
    using const_pointer          = const T*;
    using iterator               = T*;
    using const_iterator         = const T*;
    using reverse_iterator       = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // ==================== 构造 / 析构 ====================

    vector() noexcept : start_(nullptr), finish_(nullptr), end_of_storage_(nullptr) {}

    explicit vector(size_type n) {
        init_fill(n, T{});
    }

    vector(size_type n, const T& value) {
        init_fill(n, value);
    }

    template <typename InputIt,
        typename = typename std::enable_if<
            !std::is_integral<InputIt>::value>::type>
    vector(InputIt first, InputIt last) {
        init_range(first, last);
    }

    vector(std::initializer_list<T> il) {
        init_range(il.begin(), il.end());
    }

    vector(const vector& other) {
        init_range(other.begin(), other.end());
    }

    vector(vector&& other) noexcept
        : start_(other.start_), finish_(other.finish_),
          end_of_storage_(other.end_of_storage_) {
        other.start_ = other.finish_ = other.end_of_storage_ = nullptr;
    }

    ~vector() { destroy_and_free(); }

    // ==================== 赋值 ====================

    vector& operator=(const vector& other) {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            destroy_and_free();
            start_ = other.start_;
            finish_ = other.finish_;
            end_of_storage_ = other.end_of_storage_;
            other.start_ = other.finish_ = other.end_of_storage_ = nullptr;
        }
        return *this;
    }

    vector& operator=(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
        return *this;
    }

    void assign(size_type n, const T& value) {
        clear();
        if (n > capacity()) {
            destroy_and_free();
            allocate_and_fill(n, value);
        } else {
            uninitialized_fill_n(start_, n, value);
            finish_ = start_ + n;
        }
    }

    template <typename InputIt,
        typename = typename std::enable_if<
            !std::is_integral<InputIt>::value>::type>
    void assign(InputIt first, InputIt last) {
        clear();
        for (; first != last; ++first) push_back(*first);
    }

    // ==================== 迭代器 ====================

    iterator       begin()  noexcept { return start_; }
    const_iterator begin()  const noexcept { return start_; }
    const_iterator cbegin() const noexcept { return start_; }

    iterator       end()    noexcept { return finish_; }
    const_iterator end()    const noexcept { return finish_; }
    const_iterator cend()   const noexcept { return finish_; }

    reverse_iterator       rbegin()  noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator       rend()    noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }

    // ==================== 容量 ====================

    size_type size()     const noexcept { return static_cast<size_type>(finish_ - start_); }
    size_type capacity() const noexcept { return static_cast<size_type>(end_of_storage_ - start_); }
    bool      empty()    const noexcept { return start_ == finish_; }

    void reserve(size_type n) {
        if (n > capacity()) reallocate(n);
    }

    void shrink_to_fit() {
        if (finish_ != end_of_storage_) reallocate(size());
    }

    // ==================== 元素访问 ====================

    reference       operator[](size_type i)       { return start_[i]; }
    const_reference operator[](size_type i) const { return start_[i]; }

    reference at(size_type i) {
        if (i >= size()) throw std::out_of_range("vector::at");
        return start_[i];
    }
    const_reference at(size_type i) const {
        if (i >= size()) throw std::out_of_range("vector::at");
        return start_[i];
    }

    reference       front()       { return *start_; }
    const_reference front() const { return *start_; }
    reference       back()        { return *(finish_ - 1); }
    const_reference back()  const { return *(finish_ - 1); }

    pointer       data()       noexcept { return start_; }
    const_pointer data() const noexcept { return start_; }

    // ==================== 修改操作 ====================

    void push_back(const T& value) {
        if (finish_ == end_of_storage_) grow();
        construct(finish_, value);
        ++finish_;
    }

    void push_back(T&& value) {
        if (finish_ == end_of_storage_) grow();
        construct(finish_, std::move(value));
        ++finish_;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (finish_ == end_of_storage_) grow();
        construct(finish_, std::forward<Args>(args)...);
        ++finish_;
    }

    void pop_back() {
        --finish_;
        destroy(finish_);
    }

    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator emplace(const_iterator cpos, Args&&... args) {
        size_type offset = cpos - start_;
        if (finish_ == end_of_storage_) grow();
        iterator pos = start_ + offset;
        // 将 [pos, finish_) 整体后移一格
        if (pos != finish_) {
            construct(finish_, std::move(*(finish_ - 1)));
            std::move_backward(pos, finish_ - 1, finish_);
            destroy(pos);
        }
        construct(pos, std::forward<Args>(args)...);
        ++finish_;
        return pos;
    }

    iterator erase(const_iterator cpos) {
        iterator pos = start_ + (cpos - start_);
        std::move(pos + 1, finish_, pos);
        --finish_;
        destroy(finish_);
        return pos;
    }

    iterator erase(const_iterator cfirst, const_iterator clast) {
        iterator first = start_ + (cfirst - start_);
        iterator last  = start_ + (clast  - start_);
        iterator new_end = std::move(last, finish_, first);
        destroy(new_end, finish_);
        finish_ = new_end;
        return first;
    }

    void clear() noexcept {
        destroy(start_, finish_);
        finish_ = start_;
    }

    void resize(size_type n) { resize(n, T{}); }
    void resize(size_type n, const T& value) {
        if (n < size()) {
            erase(begin() + n, end());
        } else if (n > size()) {
            if (n > capacity()) reallocate(n);
            uninitialized_fill_n(finish_, n - size(), value);
            finish_ = start_ + n;
        }
    }

    void swap(vector& other) noexcept {
        std::swap(start_, other.start_);
        std::swap(finish_, other.finish_);
        std::swap(end_of_storage_, other.end_of_storage_);
    }

private:
    pointer start_;
    pointer finish_;
    pointer end_of_storage_;
    Alloc   alloc_;

    void init_fill(size_type n, const T& value) {
        start_  = alloc_.allocate(n);
        finish_ = start_ + n;
        end_of_storage_ = finish_;
        uninitialized_fill(start_, finish_, value);
    }

    template <typename InputIt>
    void init_range(InputIt first, InputIt last) {
        size_type n = static_cast<size_type>(mystl::distance(first, last));
        start_  = alloc_.allocate(n);
        finish_ = start_ + n;
        end_of_storage_ = finish_;
        uninitialized_copy(first, last, start_);
    }

    void allocate_and_fill(size_type n, const T& value) {
        start_  = alloc_.allocate(n);
        finish_ = start_ + n;
        end_of_storage_ = finish_;
        uninitialized_fill(start_, finish_, value);
    }

    void destroy_and_free() {
        if (start_) {
            destroy(start_, finish_);
            alloc_.deallocate(start_, capacity());
            start_ = finish_ = end_of_storage_ = nullptr;
        }
    }

    // 扩容：容量翻倍（至少为 1）
    void grow() {
        reallocate(std::max(capacity() * 2, size_type(1)));
    }

    void reallocate(size_type new_cap) {
        size_type old_size = size();
        pointer new_start = alloc_.allocate(new_cap);
        // 搬移旧元素
        for (size_type i = 0; i < old_size; ++i) {
            construct(new_start + i, std::move(start_[i]));
        }
        destroy_and_free();
        start_  = new_start;
        finish_ = start_ + old_size;
        end_of_storage_ = start_ + new_cap;
    }
};

// 非成员比较
template <typename T, typename A>
bool operator==(const vector<T,A>& a, const vector<T,A>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
    return true;
}
template <typename T, typename A>
bool operator!=(const vector<T,A>& a, const vector<T,A>& b) { return !(a == b); }

} // namespace mystl
