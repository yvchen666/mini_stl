#pragma once
// reverse_iterator.h — 反向迭代器适配器
//
// reverse_iterator 把一个双向/随机迭代器"翻转"：
//   ++reverse_it  →  --underlying_it
//   --reverse_it  →  ++underlying_it
//
// 关键细节：operator* 的实现
//   直觉上以为 *rev = *cur，但这是错的！
//   reverse_iterator 用于 [rbegin, rend) 区间，
//   其中 rbegin() = reverse_iterator(end())。
//   如果 *rbegin 直接解引用 end()，就是未定义行为。
//
//   正确做法：*rev = *(cur - 1)
//   即先退一步，再解引用。
//
// 举例：
//   vector: [1, 2, 3, 4]
//              ^           ^ end
//   rbegin = reverse_iterator(end) = *(end-1) = 4  ✓
//   rend   = reverse_iterator(begin)，不可解引用     ✓

#include "iterator_traits.h"
#include "iterator_algo.h"

namespace mystl {

template <typename Iterator>
class reverse_iterator {
public:
    using iterator_type     = Iterator;
    using iterator_category = typename iterator_traits<Iterator>::iterator_category;
    using value_type        = typename iterator_traits<Iterator>::value_type;
    using difference_type   = typename iterator_traits<Iterator>::difference_type;
    using pointer           = typename iterator_traits<Iterator>::pointer;
    using reference         = typename iterator_traits<Iterator>::reference;

    // 构造
    reverse_iterator() = default;
    explicit reverse_iterator(iterator_type it) : current_(it) {}

    // 从另一种（兼容的）reverse_iterator 转换
    template <typename U>
    reverse_iterator(const reverse_iterator<U>& other)
        : current_(other.base()) {}

    // 返回底层迭代器（指向当前逻辑位置的下一个元素）
    iterator_type base() const { return current_; }

    // operator* ：先退一步，再解引用
    reference operator*() const {
        Iterator tmp = current_;
        return *--tmp;
    }

    pointer operator->() const {
        return &(operator*());
    }

    // 随机访问
    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    // 前进反向 = 后退正向
    reverse_iterator& operator++() { --current_; return *this; }
    reverse_iterator  operator++(int) {
        reverse_iterator tmp = *this;
        --current_;
        return tmp;
    }
    reverse_iterator& operator--() { ++current_; return *this; }
    reverse_iterator  operator--(int) {
        reverse_iterator tmp = *this;
        ++current_;
        return tmp;
    }

    reverse_iterator operator+(difference_type n) const {
        return reverse_iterator(current_ - n);
    }
    reverse_iterator& operator+=(difference_type n) {
        current_ -= n;
        return *this;
    }
    reverse_iterator operator-(difference_type n) const {
        return reverse_iterator(current_ + n);
    }
    reverse_iterator& operator-=(difference_type n) {
        current_ += n;
        return *this;
    }

private:
    Iterator current_;
};

// 非成员比较运算符
template <typename It1, typename It2>
bool operator==(const reverse_iterator<It1>& a,
                const reverse_iterator<It2>& b) {
    return a.base() == b.base();
}
template <typename It1, typename It2>
bool operator!=(const reverse_iterator<It1>& a,
                const reverse_iterator<It2>& b) {
    return !(a == b);
}
template <typename It1, typename It2>
bool operator<(const reverse_iterator<It1>& a,
               const reverse_iterator<It2>& b) {
    return b.base() < a.base();  // 注意：方向相反
}
template <typename It1, typename It2>
bool operator<=(const reverse_iterator<It1>& a,
                const reverse_iterator<It2>& b) {
    return !(b < a);
}
template <typename It1, typename It2>
bool operator>(const reverse_iterator<It1>& a,
               const reverse_iterator<It2>& b) {
    return b < a;
}
template <typename It1, typename It2>
bool operator>=(const reverse_iterator<It1>& a,
                const reverse_iterator<It2>& b) {
    return !(a < b);
}

// n + rev_it
template <typename It>
reverse_iterator<It>
operator+(typename reverse_iterator<It>::difference_type n,
          const reverse_iterator<It>& it) {
    return reverse_iterator<It>(it.base() - n);
}

// rev_it1 - rev_it2
template <typename It1, typename It2>
auto operator-(const reverse_iterator<It1>& a,
               const reverse_iterator<It2>& b)
    -> decltype(b.base() - a.base()) {
    return b.base() - a.base();
}

} // namespace mystl
