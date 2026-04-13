#pragma once
// map_set.h — map / set / multimap / multiset
//
// 全部基于 rb_tree，只是在以下几点上有差异：
//   set/map    → insert_unique，不允许重复 key
//   multiset/multimap → insert_equal，允许重复 key
//
//   set/multiset      → value_type == key_type，KeyOfValue = identity
//   map/multimap      → value_type == pair<const Key, Value>，KeyOfValue 取 first

#include "rb_tree.h"
#include <utility>  // std::pair

namespace mystl {

// ==================== set ====================
template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = Allocator<rb_node<Key>>>
class set {
    using tree_type = rb_tree<Key, Key, Identity<Key>, Compare, Alloc>;
    tree_type tree_;
public:
    using key_type        = Key;
    using value_type      = Key;
    using size_type       = typename tree_type::size_type;
    using iterator        = typename tree_type::const_iterator;  // set 迭代器不可修改
    using const_iterator  = typename tree_type::const_iterator;

    set() = default;
    set(std::initializer_list<Key> il) { for (const auto& k : il) insert(k); }

    template <typename InputIt>
    set(InputIt first, InputIt last) { for (; first != last; ++first) insert(*first); }

    iterator begin()  const { return tree_.cbegin(); }
    iterator end()    const { return tree_.cend(); }
    bool      empty() const { return tree_.empty(); }
    size_type size()  const { return tree_.size(); }

    std::pair<iterator, bool> insert(const Key& k) {
        auto [it, ok] = tree_.insert_unique(k);
        return {it, ok};
    }
    size_type erase(const Key& k) { return tree_.erase(k); }
    void      clear()              { tree_.clear(); }

    iterator  find(const Key& k)         const { return tree_.find(k); }
    size_type count(const Key& k)        const { return tree_.count(k); }
    iterator  lower_bound(const Key& k)  const {
        return const_cast<tree_type&>(tree_).lower_bound(k);
    }
    iterator  upper_bound(const Key& k)  const {
        return const_cast<tree_type&>(tree_).upper_bound(k);
    }
};

// ==================== multiset ====================
template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = Allocator<rb_node<Key>>>
class multiset {
    using tree_type = rb_tree<Key, Key, Identity<Key>, Compare, Alloc>;
    tree_type tree_;
public:
    using key_type       = Key;
    using value_type     = Key;
    using size_type      = typename tree_type::size_type;
    using iterator       = typename tree_type::const_iterator;
    using const_iterator = typename tree_type::const_iterator;

    multiset() = default;
    multiset(std::initializer_list<Key> il) { for (const auto& k : il) insert(k); }

    iterator begin()  const { return tree_.cbegin(); }
    iterator end()    const { return tree_.cend(); }
    bool      empty() const { return tree_.empty(); }
    size_type size()  const { return tree_.size(); }

    iterator  insert(const Key& k)  { return tree_.insert_equal(k); }
    size_type erase(const Key& k)   { return tree_.erase(k); }
    void      clear()               { tree_.clear(); }

    iterator  find(const Key& k)        const { return tree_.find(k); }
    size_type count(const Key& k)       const { return tree_.count(k); }
    iterator  lower_bound(const Key& k) const {
        return const_cast<tree_type&>(tree_).lower_bound(k);
    }
    iterator  upper_bound(const Key& k) const {
        return const_cast<tree_type&>(tree_).upper_bound(k);
    }
};

// ==================== map ====================
template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = Allocator<rb_node<std::pair<const Key, T>>>>
class map {
    using PairType  = std::pair<const Key, T>;
    using tree_type = rb_tree<Key, PairType, SelectFirst<PairType>, Compare, Alloc>;
    tree_type tree_;
public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = PairType;
    using size_type       = typename tree_type::size_type;
    using iterator        = typename tree_type::iterator;
    using const_iterator  = typename tree_type::const_iterator;

    map() = default;
    map(std::initializer_list<PairType> il) {
        for (const auto& p : il) insert(p);
    }

    template <typename InputIt>
    map(InputIt first, InputIt last) { for (; first != last; ++first) insert(*first); }

    iterator       begin()  { return tree_.begin(); }
    iterator       end()    { return tree_.end(); }
    const_iterator begin()  const { return tree_.cbegin(); }
    const_iterator end()    const { return tree_.cend(); }
    const_iterator cbegin() const { return tree_.cbegin(); }
    const_iterator cend()   const { return tree_.cend(); }
    bool      empty()  const { return tree_.empty(); }
    size_type size()   const { return tree_.size(); }

    std::pair<iterator, bool> insert(const PairType& p) {
        return tree_.insert_unique(p);
    }

    // operator[]：找到则返回引用，找不到则插入默认值
    T& operator[](const Key& k) {
        auto it = tree_.lower_bound(k);
        if (it == tree_.end() || Compare{}(k, it->first)) {
            it = tree_.insert_unique({k, T{}}).first;
        }
        return const_cast<T&>(it->second);
    }

    T& operator[](Key&& k) {
        auto it = tree_.lower_bound(k);
        if (it == tree_.end() || Compare{}(k, it->first)) {
            it = tree_.insert_unique({std::move(k), T{}}).first;
        }
        return const_cast<T&>(it->second);
    }

    T& at(const Key& k) {
        auto it = tree_.find(k);
        if (it == tree_.end()) throw std::out_of_range("map::at");
        return const_cast<T&>(it->second);
    }

    size_type erase(const Key& k) { return tree_.erase(k); }
    void      erase(iterator pos) { tree_.erase(pos); }
    void      clear()             { tree_.clear(); }

    iterator  find(const Key& k)  { return tree_.find(k); }
    const_iterator find(const Key& k) const { return tree_.find(k); }
    size_type count(const Key& k) const { return tree_.count(k); }
    iterator  lower_bound(const Key& k) { return tree_.lower_bound(k); }
    iterator  upper_bound(const Key& k) { return tree_.upper_bound(k); }
};

// 给 map 添加 key_comp 访问
// （内嵌在 tree_ 里，这里做个透传）

// ==================== multimap ====================
template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = Allocator<rb_node<std::pair<const Key, T>>>>
class multimap {
    using PairType  = std::pair<const Key, T>;
    using tree_type = rb_tree<Key, PairType, SelectFirst<PairType>, Compare, Alloc>;
    tree_type tree_;
public:
    using key_type       = Key;
    using mapped_type    = T;
    using value_type     = PairType;
    using size_type      = typename tree_type::size_type;
    using iterator       = typename tree_type::iterator;
    using const_iterator = typename tree_type::const_iterator;

    multimap() = default;
    multimap(std::initializer_list<PairType> il) { for (const auto& p : il) insert(p); }

    iterator       begin()  { return tree_.begin(); }
    iterator       end()    { return tree_.end(); }
    const_iterator begin()  const { return tree_.cbegin(); }
    const_iterator end()    const { return tree_.cend(); }
    bool      empty()  const { return tree_.empty(); }
    size_type size()   const { return tree_.size(); }

    iterator  insert(const PairType& p) { return tree_.insert_equal(p); }
    size_type erase(const Key& k) { return tree_.erase(k); }
    void      clear()             { tree_.clear(); }

    iterator  find(const Key& k)  { return tree_.find(k); }
    size_type count(const Key& k) const { return tree_.count(k); }
    iterator  lower_bound(const Key& k) { return tree_.lower_bound(k); }
    iterator  upper_bound(const Key& k) { return tree_.upper_bound(k); }
};

} // namespace mystl
