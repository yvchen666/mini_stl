#pragma once
// hashtable.h — 开链法哈希表
// hash_set / hash_map — 基于 hashtable 的无序容器
//
// 实现策略：
//   - 桶数组 (buckets_)：存指向链表头节点的指针
//   - 每个桶的链表是一条单向链表（用于解决冲突）
//   - 负载因子 = size_ / buckets_.size()，超过 1.0 时重哈希
//   - 桶的数量始终取素数（提高哈希均匀性）

#include "../phase01_allocator/alloc.h"
#include "../phase01_allocator/construct.h"
#include "../phase02_iterator/iterator.h"
#include "key_extractors.h"
#include <functional>
#include <vector>
#include <cstdlib>

namespace mystl {

// ---------- 素数桶大小表 ----------
static const std::size_t prime_list[] = {
    53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
    49157, 98317, 196613, 393241, 786433, 1572869, 3145739,
    6291469, 12582917, 25165843, 50331653, 100663319
};
static const std::size_t num_primes = sizeof(prime_list) / sizeof(prime_list[0]);

inline std::size_t next_prime(std::size_t n) {
    for (std::size_t i = 0; i < num_primes; ++i)
        if (prime_list[i] >= n) return prime_list[i];
    return prime_list[num_primes - 1];
}

// ---------- 哈希表节点 ----------
template <typename T>
struct ht_node {
    ht_node* next;
    T        value;
};

// ---------- 哈希表迭代器 ----------
template <typename T, typename Hash, typename KeyEq, typename KeyOf>
class hashtable;  // 前向声明

template <typename T, typename Ref, typename Ptr,
          typename Hash, typename KeyEq, typename KeyOf>
struct ht_iterator
    : iterator<forward_iterator_tag, T, std::ptrdiff_t, Ptr, Ref>
{
    using self    = ht_iterator<T, Ref, Ptr, Hash, KeyEq, KeyOf>;
    using iterator = ht_iterator<T, T&, T*, Hash, KeyEq, KeyOf>;
    using HT      = hashtable<T, Hash, KeyEq, KeyOf>;
    using node_ptr = ht_node<T>*;

    node_ptr cur_;
    const HT* ht_;

    ht_iterator() : cur_(nullptr), ht_(nullptr) {}
    ht_iterator(node_ptr n, const HT* h) : cur_(n), ht_(h) {}
    ht_iterator(const iterator& o) : cur_(o.cur_), ht_(o.ht_) {}

    bool operator==(const self& o) const { return cur_ == o.cur_; }
    bool operator!=(const self& o) const { return cur_ != o.cur_; }

    Ref operator*()  const { return cur_->value; }
    Ptr operator->() const { return &cur_->value; }

    self& operator++() {
        node_ptr old = cur_;
        cur_ = cur_->next;
        if (!cur_) {
            // 当前链表走完，找下一个非空桶
            std::size_t idx = ht_->bucket_index(old->value) + 1;
            while (idx < ht_->buckets_.size() && !ht_->buckets_[idx])
                ++idx;
            cur_ = (idx < ht_->buckets_.size()) ? ht_->buckets_[idx] : nullptr;
        }
        return *this;
    }
    self operator++(int) { auto t = *this; ++*this; return t; }
};

// ---------- hashtable ----------
template <typename T,
          typename Hash   = std::hash<T>,
          typename KeyEq  = std::equal_to<T>,
          typename KeyOf  = Identity<T>>
class hashtable {
public:
    using value_type = T;
    using size_type  = std::size_t;
    using iterator       = ht_iterator<T, T&, T*, Hash, KeyEq, KeyOf>;
    using const_iterator = ht_iterator<T, const T&, const T*, Hash, KeyEq, KeyOf>;

    // 让迭代器能访问 buckets_ 和 bucket_index
    template <typename, typename, typename, typename, typename, typename>
    friend struct ht_iterator;

    explicit hashtable(std::size_t n = 53) {
        buckets_.resize(next_prime(n), nullptr);
    }
    ~hashtable() { clear(); }

    // ==================== 基本信息 ====================
    bool      empty()    const { return size_ == 0; }
    size_type size()     const { return size_; }
    size_type bucket_count() const { return buckets_.size(); }

    // ==================== 迭代器 ====================
    iterator begin() {
        for (auto* nd : buckets_)
            if (nd) return {nd, this};
        return end();
    }
    const_iterator begin() const {
        for (auto* nd : buckets_)
            if (nd) return {nd, this};
        return end();
    }
    iterator       end()       { return {nullptr, this}; }
    const_iterator end() const { return {nullptr, this}; }

    // ==================== 插入 ====================

    // insert_unique：key 不重复
    std::pair<iterator, bool> insert_unique(const T& v) {
        rehash_if_needed();
        std::size_t idx = bucket_index(v);
        for (auto* nd = buckets_[idx]; nd; nd = nd->next) {
            if (eq_(kof_(nd->value), kof_(v)))
                return {{nd, this}, false};  // 已存在
        }
        return {insert_at(idx, v), true};
    }

    // insert_equal：允许重复
    iterator insert_equal(const T& v) {
        rehash_if_needed();
        std::size_t idx = bucket_index(v);
        return insert_at(idx, v);
    }

    // ==================== 查找 ====================
    template <typename Key>
    iterator find(const Key& k) {
        std::size_t idx = key_index(k);
        for (auto* nd = buckets_[idx]; nd; nd = nd->next)
            if (eq_(kof_(nd->value), k))
                return {nd, this};
        return end();
    }

    template <typename Key>
    size_type count(const Key& k) const {
        std::size_t idx = key_index(k);
        size_type n = 0;
        for (auto* nd = buckets_[idx]; nd; nd = nd->next)
            if (eq_(kof_(nd->value), k)) ++n;
        return n;
    }

    // ==================== 删除 ====================
    template <typename Key>
    size_type erase(const Key& k) {
        std::size_t idx = key_index(k);
        ht_node<T>** pp = &buckets_[idx];
        size_type n = 0;
        while (*pp) {
            if (eq_(kof_((*pp)->value), k)) {
                auto* del = *pp;
                *pp = del->next;
                free_node(del);
                ++n; --size_;
            } else {
                pp = &(*pp)->next;
            }
        }
        return n;
    }

    void erase(iterator pos) {
        std::size_t idx = bucket_index(*pos);
        ht_node<T>** pp = &buckets_[idx];
        while (*pp && *pp != pos.cur_) pp = &(*pp)->next;
        if (*pp) { *pp = pos.cur_->next; free_node(pos.cur_); --size_; }
    }

    void clear() {
        for (auto& bucket : buckets_) {
            auto* nd = bucket;
            while (nd) { auto* nx = nd->next; free_node(nd); nd = nx; }
            bucket = nullptr;
        }
        size_ = 0;
    }

    // 供迭代器访问
    std::size_t bucket_index(const T& v) const {
        return hash_(kof_(v)) % buckets_.size();
    }

private:
    using NodeAlloc = Allocator<ht_node<T>>;

    std::vector<ht_node<T>*> buckets_;
    size_type   size_ = 0;
    Hash        hash_;
    KeyEq       eq_;
    KeyOf       kof_;
    NodeAlloc   alloc_;

    template <typename Key>
    std::size_t key_index(const Key& k) const {
        return hash_(k) % buckets_.size();
    }

    ht_node<T>* new_node(const T& v) {
        auto* nd = alloc_.allocate(1);
        alloc_.construct(nd, ht_node<T>{nullptr, v});
        return nd;
    }

    void free_node(ht_node<T>* nd) {
        alloc_.destroy(nd);
        alloc_.deallocate(nd, 1);
    }

    iterator insert_at(std::size_t idx, const T& v) {
        auto* nd = new_node(v);
        nd->next = buckets_[idx];
        buckets_[idx] = nd;
        ++size_;
        return {nd, this};
    }

    void rehash_if_needed() {
        if (size_ >= buckets_.size()) {
            std::size_t new_n = next_prime(buckets_.size() * 2);
            std::vector<ht_node<T>*> new_buckets(new_n, nullptr);
            for (auto* nd : buckets_) {
                while (nd) {
                    auto* nx = nd->next;
                    std::size_t idx = hash_(kof_(nd->value)) % new_n;
                    nd->next = new_buckets[idx];
                    new_buckets[idx] = nd;
                    nd = nx;
                }
            }
            buckets_.swap(new_buckets);
        }
    }
};

// ==================== hash_set ====================
template <typename T,
          typename Hash  = std::hash<T>,
          typename KeyEq = std::equal_to<T>>
class hash_set {
    using HT = hashtable<T, Hash, KeyEq, Identity<T>>;
    HT ht_;
public:
    using iterator       = typename HT::iterator;
    using const_iterator = typename HT::const_iterator;
    using size_type      = typename HT::size_type;

    hash_set() = default;
    hash_set(std::initializer_list<T> il) { for (const auto& v : il) insert(v); }

    std::pair<iterator, bool> insert(const T& v) { return ht_.insert_unique(v); }
    size_type erase(const T& k) { return ht_.erase(k); }
    iterator  find(const T& k)  { return ht_.find(k); }
    size_type count(const T& k) const { return ht_.count(k); }
    bool      empty() const { return ht_.empty(); }
    size_type size()  const { return ht_.size(); }
    iterator  begin() { return ht_.begin(); }
    iterator  end()   { return ht_.end(); }
};

// ==================== hash_map ====================
template <typename Key, typename T,
          typename Hash  = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>>
class hash_map {
    using PairType = std::pair<const Key, T>;
    using HT = hashtable<PairType, Hash, KeyEq, SelectFirst<PairType>>;
    HT ht_;
public:
    using iterator       = typename HT::iterator;
    using const_iterator = typename HT::const_iterator;
    using size_type      = typename HT::size_type;

    hash_map() = default;

    std::pair<iterator, bool> insert(const PairType& p) {
        return ht_.insert_unique(p);
    }
    T& operator[](const Key& k) {
        auto [it, ok] = ht_.insert_unique({k, T{}});
        return const_cast<T&>(it->second);
    }
    iterator  find(const Key& k)  { return ht_.find(k); }
    size_type erase(const Key& k) { return ht_.erase(k); }
    bool      empty() const { return ht_.empty(); }
    size_type size()  const { return ht_.size(); }
    iterator  begin() { return ht_.begin(); }
    iterator  end()   { return ht_.end(); }
};

} // namespace mystl
