#pragma once
// deque.h — 双端队列（分段连续存储）
//
// 内部结构：
//   map_   → 指针数组，每个元素指向一个固定大小的"缓冲区"(buffer)
//   每个 buffer 存储 BUF_SIZE 个 T 类型元素
//   start_, finish_ → 两个迭代器，分别标记 [begin, end)
//
// deque 迭代器维护 4 个指针：
//   cur       → 当前元素
//   first     → 当前 buffer 起始
//   last      → 当前 buffer 末尾（past-the-end）
//   node      → 指向 map_ 中对应槽位（map_[i] == buffer 指针）
//
// 跨 buffer 操作：当 cur 走到 last 时，通过 node+1 跳到下一个 buffer
//
// 扩容：当 map_ 前端或后端用完时，重新分配 map_ 并居中排列

#include "../phase01_allocator/alloc.h"
#include "../phase01_allocator/construct.h"
#include "../phase01_allocator/uninitialized.h"
#include "../phase02_iterator/iterator.h"
#include <algorithm>
#include <stdexcept>

namespace mystl {

// 每个 buffer 存放的元素个数（元素较大时减少，较小时增多）
template <typename T>
constexpr std::size_t deque_buf_size() {
    return sizeof(T) < 256 ? (512 / sizeof(T)) : std::size_t(1);
}

// ==================== deque 迭代器 ====================
template <typename T, typename Ref, typename Ptr>
struct deque_iterator
    : iterator<random_access_iterator_tag, T, std::ptrdiff_t, Ptr, Ref>
{
    using self     = deque_iterator<T, Ref, Ptr>;
    using iterator = deque_iterator<T, T&, T*>;

    static constexpr std::size_t BUF = deque_buf_size<T>();

    T*  cur;    // 当前元素
    T*  first;  // buffer 起始
    T*  last;   // buffer 末尾（past-the-end）
    T** node;   // 指向 map 中的槽位

    deque_iterator() : cur(nullptr), first(nullptr), last(nullptr), node(nullptr) {}
    deque_iterator(T* c, T** n)
        : cur(c), first(*n), last(*n + BUF), node(n) {}

    // 允许从 non-const 转 const
    deque_iterator(const iterator& o)
        : cur(o.cur), first(o.first), last(o.last), node(o.node) {}

    void set_node(T** new_node) {
        node  = new_node;
        first = *new_node;
        last  = first + BUF;
    }

    Ref operator*()  const { return *cur; }
    Ptr operator->() const { return cur; }

    self& operator++() {
        ++cur;
        if (cur == last) {           // 走到 buffer 末尾，跳到下一个
            set_node(node + 1);
            cur = first;
        }
        return *this;
    }
    self operator++(int) { auto t = *this; ++*this; return t; }

    self& operator--() {
        if (cur == first) {          // 走到 buffer 起始，跳到上一个
            set_node(node - 1);
            cur = last;
        }
        --cur;
        return *this;
    }
    self operator--(int) { auto t = *this; --*this; return t; }

    self& operator+=(std::ptrdiff_t n) {
        std::ptrdiff_t offset = n + (cur - first);
        if (offset >= 0 && offset < std::ptrdiff_t(BUF)) {
            cur += n;                // 还在当前 buffer
        } else {
            // 计算跨越多少个 buffer
            std::ptrdiff_t node_offset =
                offset > 0
                ? offset / std::ptrdiff_t(BUF)
                : -std::ptrdiff_t((-offset - 1) / BUF) - 1;
            set_node(node + node_offset);
            cur = first + (offset - node_offset * std::ptrdiff_t(BUF));
        }
        return *this;
    }
    self operator+(std::ptrdiff_t n) const { auto t = *this; return t += n; }

    self& operator-=(std::ptrdiff_t n) { return *this += -n; }
    self operator-(std::ptrdiff_t n) const { auto t = *this; return t -= n; }

    std::ptrdiff_t operator-(const self& o) const {
        return std::ptrdiff_t(BUF) * (node - o.node - 1)
               + (cur - first)
               + (o.last - o.cur);
    }

    Ref operator[](std::ptrdiff_t n) const { return *(*this + n); }

    bool operator==(const self& o) const { return cur == o.cur; }
    bool operator!=(const self& o) const { return cur != o.cur; }
    bool operator< (const self& o) const {
        return node == o.node ? cur < o.cur : node < o.node;
    }
    bool operator<=(const self& o) const { return !(o < *this); }
    bool operator> (const self& o) const { return o < *this; }
    bool operator>=(const self& o) const { return !(*this < o); }
};

template <typename T, typename Ref, typename Ptr>
deque_iterator<T,Ref,Ptr>
operator+(std::ptrdiff_t n, const deque_iterator<T,Ref,Ptr>& it) {
    return it + n;
}

// ==================== deque 容器 ====================
template <typename T, typename Alloc = Allocator<T>>
class deque {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = deque_iterator<T, T&, T*>;
    using const_iterator  = deque_iterator<T, const T&, const T*>;
    using reverse_iterator       = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    static constexpr std::size_t BUF = deque_buf_size<T>();

    deque() { init_map(0); }
    explicit deque(size_type n) { init_map(n); fill_init(n, T{}); }
    deque(size_type n, const T& v) { init_map(n); fill_init(n, v); }
    deque(std::initializer_list<T> il) {
        init_map(il.size());
        size_type i = 0;
        for (const auto& v : il) {
            construct(start_.cur + i, v);
            if (++i % BUF == 0 && i < il.size()) {
                // crossing buffer boundary during init — just push_back after
            }
        }
        // simpler: just use push_back
        clear();
        for (const auto& v : il) push_back(v);
    }
    deque(const deque& other) {
        init_map(other.size());
        uninitialized_copy(other.begin(), other.end(), start_);
        finish_ = start_ + static_cast<std::ptrdiff_t>(other.size());
    }
    deque(deque&& other) noexcept
        : map_(other.map_), map_size_(other.map_size_),
          start_(other.start_), finish_(other.finish_) {
        other.map_ = nullptr; other.map_size_ = 0;
    }
    ~deque() { free_all(); }

    deque& operator=(const deque& o) {
        if (this != &o) { clear(); for (const auto& v : o) push_back(v); }
        return *this;
    }
    deque& operator=(deque&& o) noexcept {
        if (this != &o) { free_all(); map_=o.map_; map_size_=o.map_size_;
            start_=o.start_; finish_=o.finish_; o.map_=nullptr; o.map_size_=0; }
        return *this;
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
    size_type size()  const noexcept { return static_cast<size_type>(finish_ - start_); }
    bool      empty() const noexcept { return start_ == finish_; }

    // ==================== 元素访问 ====================
    reference       operator[](size_type i)       { return start_[static_cast<std::ptrdiff_t>(i)]; }
    const_reference operator[](size_type i) const { return start_[static_cast<std::ptrdiff_t>(i)]; }
    reference at(size_type i) {
        if (i >= size()) throw std::out_of_range("deque::at");
        return (*this)[i];
    }
    reference       front()       { return *start_; }
    const_reference front() const { return *start_; }
    reference       back()        {
        auto t = finish_; --t; return *t;
    }
    const_reference back() const {
        auto t = finish_; --t; return *t;
    }

    // ==================== push / pop ====================
    void push_back(const T& v) {
        if (finish_.cur != finish_.last - 1) {
            construct(finish_.cur, v);
            ++finish_.cur;
        } else {
            push_back_aux(v);
        }
    }
    void push_back(T&& v) {
        if (finish_.cur != finish_.last - 1) {
            construct(finish_.cur, std::move(v));
            ++finish_.cur;
        } else {
            push_back_aux(std::move(v));
        }
    }

    void push_front(const T& v) {
        if (start_.cur != start_.first) {
            --start_.cur;
            construct(start_.cur, v);
        } else {
            push_front_aux(v);
        }
    }
    void push_front(T&& v) {
        if (start_.cur != start_.first) {
            --start_.cur;
            construct(start_.cur, std::move(v));
        } else {
            push_front_aux(std::move(v));
        }
    }

    void pop_back() {
        if (finish_.cur != finish_.first) {
            --finish_.cur;
            destroy(finish_.cur);
        } else {
            // finish_ 在 buffer 起始，释放旧 buffer
            buf_alloc_.deallocate(*finish_.node, BUF);
            finish_.set_node(finish_.node - 1);
            finish_.cur = finish_.last - 1;
            destroy(finish_.cur);
        }
    }

    void pop_front() {
        if (start_.cur != start_.last - 1) {
            destroy(start_.cur);
            ++start_.cur;
        } else {
            destroy(start_.cur);
            buf_alloc_.deallocate(*start_.node, BUF);
            start_.set_node(start_.node + 1);
            start_.cur = start_.first;
        }
    }

    void clear() noexcept {
        // 析构所有元素
        for (auto nd = start_.node + 1; nd < finish_.node; ++nd) {
            destroy(*nd, *nd + BUF);
            buf_alloc_.deallocate(*nd, BUF);
        }
        if (start_.node != finish_.node) {
            destroy(start_.cur, start_.last);
            destroy(finish_.first, finish_.cur);
            buf_alloc_.deallocate(*finish_.node, BUF);
        } else {
            destroy(start_.cur, finish_.cur);
        }
        finish_ = start_;
    }

    void swap(deque& o) noexcept {
        std::swap(map_, o.map_); std::swap(map_size_, o.map_size_);
        std::swap(start_, o.start_); std::swap(finish_, o.finish_);
    }

private:
    using MapAlloc = typename Alloc::template rebind<T*>::other;
    using BufAlloc = Alloc;

    T**       map_;
    size_type map_size_;
    iterator  start_;
    iterator  finish_;
    MapAlloc  map_alloc_;
    BufAlloc  buf_alloc_;

    T* alloc_buf() { return buf_alloc_.allocate(BUF); }

    void init_map(size_type n_elems) {
        constexpr size_type INITIAL_MAP_SIZE = 8;
        size_type n_bufs = n_elems / BUF + 1;
        map_size_ = std::max(INITIAL_MAP_SIZE, n_bufs + 2);
        map_ = map_alloc_.allocate(map_size_);
        // 居中排列
        T** nstart = map_ + (map_size_ - n_bufs) / 2;
        T** nfinish = nstart + n_bufs;
        for (T** cur = nstart; cur < nfinish; ++cur)
            *cur = alloc_buf();
        start_.set_node(nstart);
        finish_.set_node(nfinish - 1);
        start_.cur  = start_.first;
        finish_.cur = finish_.first + n_elems % BUF;
    }

    void fill_init(size_type n, const T& v) {
        for (auto nd = start_.node; nd < finish_.node; ++nd)
            uninitialized_fill(*nd, *nd + BUF, v);
        uninitialized_fill(finish_.first, finish_.cur, v);
    }

    void free_all() {
        if (!map_) return;
        for (auto nd = start_.node; nd <= finish_.node; ++nd)
            if (*nd) buf_alloc_.deallocate(*nd, BUF);
        map_alloc_.deallocate(map_, map_size_);
        map_ = nullptr;
    }

    // 扩 map（后端）
    void reallocate_map(bool add_at_front) {
        size_type old_bufs = finish_.node - start_.node + 1;
        size_type new_bufs = old_bufs + 1;
        T** new_nstart;
        if (map_size_ > 2 * new_bufs) {
            // map 本身够大，只是重新居中
            new_nstart = map_ + (map_size_ - new_bufs) / 2
                         + (add_at_front ? 1 : 0);
            if (new_nstart < start_.node)
                std::copy(start_.node, finish_.node + 1, new_nstart);
            else
                std::copy_backward(start_.node, finish_.node + 1,
                                   new_nstart + old_bufs);
        } else {
            size_type new_map_size = map_size_ + std::max(map_size_, new_bufs) + 2;
            T** new_map = map_alloc_.allocate(new_map_size);
            new_nstart = new_map + (new_map_size - new_bufs) / 2
                         + (add_at_front ? 1 : 0);
            std::copy(start_.node, finish_.node + 1, new_nstart);
            map_alloc_.deallocate(map_, map_size_);
            map_ = new_map;
            map_size_ = new_map_size;
        }
        start_.set_node(new_nstart);
        finish_.set_node(new_nstart + old_bufs - 1);
    }

    void push_back_aux(const T& v) {
        reallocate_map(false);
        *(finish_.node + 1) = alloc_buf();
        construct(finish_.cur, v);
        finish_.set_node(finish_.node + 1);
        finish_.cur = finish_.first;
    }
    void push_back_aux(T&& v) {
        reallocate_map(false);
        *(finish_.node + 1) = alloc_buf();
        construct(finish_.cur, std::move(v));
        finish_.set_node(finish_.node + 1);
        finish_.cur = finish_.first;
    }
    void push_front_aux(const T& v) {
        reallocate_map(true);
        *(start_.node - 1) = alloc_buf();
        start_.set_node(start_.node - 1);
        start_.cur = start_.last - 1;
        construct(start_.cur, v);
    }
    void push_front_aux(T&& v) {
        reallocate_map(true);
        *(start_.node - 1) = alloc_buf();
        start_.set_node(start_.node - 1);
        start_.cur = start_.last - 1;
        construct(start_.cur, std::move(v));
    }
};

} // namespace mystl
