#pragma once
// heap.h — 堆操作算法（大顶堆）
// priority_queue.h — 基于堆的优先队列
//
// 堆算法（对随机访问迭代器）：
//   push_heap   — 将 [first, last-1) 的最后一个元素下沉/上浮到正确位置
//   pop_heap    — 将最大元素移到末尾，重建 [first, last-1) 的堆
//   make_heap   — 将任意区间整理为堆
//   sort_heap   — 堆排序：反复 pop_heap
//   is_heap     — 检查是否为合法堆
//
// 大顶堆性质：parent >= children
// 用数组下标（0-based）：parent(i) = (i-1)/2, left(i) = 2i+1, right(i) = 2i+2

#include "../phase02_iterator/iterator.h"
#include "vector.h"        // mystl::vector 作为默认 Container
#include <functional>  // std::less

namespace mystl {

// ==================== 内部辅助：上浮（sift up）====================
// 把 hole 位置的元素上浮到合适位置
template <typename RandomIt, typename Distance, typename T, typename Compare>
void sift_up(RandomIt first, Distance hole, Distance top, T value, Compare comp) {
    Distance parent = (hole - 1) / 2;
    while (hole > top && comp(*(first + parent), value)) {
        *(first + hole) = std::move(*(first + parent));
        hole = parent;
        parent = (hole - 1) / 2;
    }
    *(first + hole) = std::move(value);
}

// ==================== 内部辅助：下沉（sift down）====================
// 把 hole 位置的元素下沉到合适位置，堆大小为 len
template <typename RandomIt, typename Distance, typename T, typename Compare>
void sift_down(RandomIt first, Distance hole, Distance len, T value, Compare comp) {
    Distance child = 2 * hole + 2;  // 右孩子
    while (child < len) {
        // 选较大的孩子
        if (comp(*(first + child), *(first + (child - 1))))
            --child;
        *(first + hole) = std::move(*(first + child));
        hole = child;
        child = 2 * hole + 2;
    }
    if (child == len) {   // 只有左孩子
        *(first + hole) = std::move(*(first + (child - 1)));
        hole = child - 1;
    }
    sift_up(first, hole, Distance(0), std::move(value), comp);
}

// ==================== push_heap ====================
// 前提：[first, last-1) 已是合法堆，将 *(last-1) 加入堆
template <typename RandomIt, typename Compare>
void push_heap(RandomIt first, RandomIt last, Compare comp) {
    using Distance = typename std::iterator_traits<RandomIt>::difference_type;
    using T        = typename std::iterator_traits<RandomIt>::value_type;
    if (last - first < 2) return;
    Distance hole = (last - first) - 1;
    T value = std::move(*(last - 1));
    sift_up(first, hole, Distance(0), std::move(value), comp);
}

template <typename RandomIt>
void push_heap(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    using D = typename std::iterator_traits<RandomIt>::difference_type;
    if (last - first < 2) return;
    D hole = (last - first) - 1;
    T value = std::move(*(last - 1));
    sift_up(first, hole, D(0), std::move(value), std::less<T>{});
}

// ==================== pop_heap ====================
// 将最大元素（堆顶）交换到末尾，重建 [first, last-1) 的堆
template <typename RandomIt, typename Compare>
void pop_heap(RandomIt first, RandomIt last, Compare comp) {
    using Distance = typename std::iterator_traits<RandomIt>::difference_type;
    using T        = typename std::iterator_traits<RandomIt>::value_type;
    if (last - first < 2) return;
    T value = std::move(*(last - 1));
    *(last - 1) = std::move(*first);
    sift_down(first, Distance(0), Distance(last - first - 1),
              std::move(value), comp);
}

template <typename RandomIt>
void pop_heap(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    using D = typename std::iterator_traits<RandomIt>::difference_type;
    if (last - first < 2) return;
    T value = std::move(*(last - 1));
    *(last - 1) = std::move(*first);
    sift_down(first, D(0), D(last - first - 1), std::move(value), std::less<T>{});
}

// ==================== make_heap ====================
// 将 [first, last) 整理为堆，从最后一个非叶节点开始逐个下沉
template <typename RandomIt, typename Compare>
void make_heap(RandomIt first, RandomIt last, Compare comp) {
    using Distance = typename std::iterator_traits<RandomIt>::difference_type;
    using T        = typename std::iterator_traits<RandomIt>::value_type;
    Distance len = last - first;
    if (len < 2) return;
    for (Distance i = (len - 2) / 2; i >= 0; --i) {
        T value = std::move(*(first + i));
        sift_down(first, i, len, std::move(value), comp);
    }
}

template <typename RandomIt>
void make_heap(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    using D = typename std::iterator_traits<RandomIt>::difference_type;
    D len = last - first;
    if (len < 2) return;
    for (D i = (len - 2) / 2; i >= 0; --i) {
        T value = std::move(*(first + i));
        sift_down(first, i, len, std::move(value), std::less<T>{});
    }
}

// ==================== sort_heap ====================
// 堆排序：反复 pop_heap，结果为升序
template <typename RandomIt, typename Compare>
void sort_heap(RandomIt first, RandomIt last, Compare comp) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    using D = typename std::iterator_traits<RandomIt>::difference_type;
    while (last - first > 1) {
        T value = std::move(*(last - 1));
        *(last - 1) = std::move(*first);
        --last;
        sift_down(first, D(0), D(last - first), std::move(value), comp);
    }
}

template <typename RandomIt>
void sort_heap(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    using D = typename std::iterator_traits<RandomIt>::difference_type;
    while (last - first > 1) {
        T value = std::move(*(last - 1));
        *(last - 1) = std::move(*first);
        --last;
        sift_down(first, D(0), D(last - first), std::move(value), std::less<T>{});
    }
}

// ==================== is_heap ====================
template <typename RandomIt, typename Compare>
bool is_heap(RandomIt first, RandomIt last, Compare comp) {
    auto len = last - first;
    for (decltype(len) i = 1; i < len; ++i)
        if (comp(*(first + (i-1)/2), *(first + i)))
            return false;
    return true;
}

template <typename RandomIt>
bool is_heap(RandomIt first, RandomIt last) {
    auto len = last - first;
    for (decltype(len) i = 1; i < len; ++i)
        if (*(first + (i-1)/2) < *(first + i))
            return false;
    return true;
}
template <typename T,
          typename Container = mystl::vector<T>,
          typename Compare   = std::less<T>>
class priority_queue {
public:
    using value_type      = T;
    using container_type  = Container;
    using size_type       = typename Container::size_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

    priority_queue() = default;

    explicit priority_queue(const Compare& comp) : comp_(comp) {}

    template <typename InputIt>
    priority_queue(InputIt first, InputIt last, const Compare& comp = Compare{})
        : c_(first, last), comp_(comp) {
        // 直接用 sift_down 实现 make_heap，避免 ADL 歧义
        using Distance = typename Container::difference_type;
        Distance len = static_cast<Distance>(c_.size());
        for (Distance i = (len - 2) / 2; i >= 0; --i) {
            auto value = std::move(c_[i]);
            sift_down(c_.begin(), i, len, std::move(value), comp_);
        }
    }

    bool      empty() const { return c_.empty(); }
    size_type size()  const { return c_.size(); }
    const_reference top() const { return c_.front(); }

    void push(const T& v) {
        c_.push_back(v);
        using Distance = typename Container::difference_type;
        Distance hole = static_cast<Distance>(c_.size()) - 1;
        auto value = std::move(c_[hole]);
        sift_up(c_.begin(), hole, Distance(0), std::move(value), comp_);
    }
    void push(T&& v) {
        c_.push_back(std::move(v));
        using Distance = typename Container::difference_type;
        Distance hole = static_cast<Distance>(c_.size()) - 1;
        auto value = std::move(c_[hole]);
        sift_up(c_.begin(), hole, Distance(0), std::move(value), comp_);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        c_.emplace_back(std::forward<Args>(args)...);
        using Distance = typename Container::difference_type;
        Distance hole = static_cast<Distance>(c_.size()) - 1;
        auto value = std::move(c_[hole]);
        sift_up(c_.begin(), hole, Distance(0), std::move(value), comp_);
    }

    void pop() {
        using Distance = typename Container::difference_type;
        Distance len = static_cast<Distance>(c_.size());
        auto value = std::move(c_.back());
        c_.back() = std::move(c_.front());
        c_.pop_back();
        if (!c_.empty()) {
            sift_down(c_.begin(), Distance(0), len - 1, std::move(value), comp_);
        }
    }

    void swap(priority_queue& other) noexcept {
        std::swap(c_, other.c_);
        std::swap(comp_, other.comp_);
    }

private:
    Container c_;
    Compare   comp_;
};

} // namespace mystl
