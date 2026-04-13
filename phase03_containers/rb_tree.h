#pragma once
// rb_tree.h — 红黑树（Red-Black Tree）
//
// 五条红黑树性质：
//   1. 每个节点是红色或黑色
//   2. 根节点是黑色
//   3. 每个叶节点（NIL/哨兵）是黑色
//   4. 红色节点的两个子节点都是黑色（不存在连续红节点）
//   5. 从任意节点到叶节点的所有路径，黑色节点数相同（黑高相等）
//
// 实现细节：
//   - 使用哨兵节点 nil_（黑色，代替 nullptr）
//   - 额外维护 header_ 节点：parent 指向根，left 指向最小，right 指向最大
//   - 迭代器支持中序遍历（得到有序序列）
//   - 插入修复：3 种 case
//   - 删除修复：4 种 case

#include "../phase01_allocator/alloc.h"
#include "../phase01_allocator/construct.h"
#include "../phase02_iterator/iterator.h"
#include "key_extractors.h"
#include <functional>
#include <utility>

namespace mystl {

enum class RBColor : bool { Red = false, Black = true };

// ---------- 树节点 ----------
template <typename T>
struct rb_node {
    using ptr = rb_node*;
    RBColor color;
    ptr     parent;
    ptr     left;
    ptr     right;
    T       value;

    static ptr minimum(ptr x, ptr nil) {
        while (x->left != nil) x = x->left;
        return x;
    }
    static ptr maximum(ptr x, ptr nil) {
        while (x->right != nil) x = x->right;
        return x;
    }
};

// ---------- 迭代器（中序遍历）----------
template <typename T, typename Ref, typename Ptr>
struct rb_iterator
    : iterator<bidirectional_iterator_tag, T, std::ptrdiff_t, Ptr, Ref>
{
    using self     = rb_iterator<T, Ref, Ptr>;
    using iterator = rb_iterator<T, T&, T*>;
    using node_ptr = rb_node<T>*;

    node_ptr node_;
    node_ptr nil_;   // 哨兵节点（叶节点）
    node_ptr header_;// 哨兵头节点（parent = root）

    rb_iterator() : node_(nullptr), nil_(nullptr), header_(nullptr) {}
    rb_iterator(node_ptr n, node_ptr nil, node_ptr header)
        : node_(n), nil_(nil), header_(header) {}
    rb_iterator(const iterator& o)
        : node_(o.node_), nil_(o.nil_), header_(o.header_) {}

    bool operator==(const self& o) const { return node_ == o.node_; }
    bool operator!=(const self& o) const { return node_ != o.node_; }

    // 跨类型比较（iterator vs const_iterator）
    template <typename Ref2, typename Ptr2>
    bool operator==(const rb_iterator<T, Ref2, Ptr2>& o) const {
        return node_ == o.node_;
    }
    template <typename Ref2, typename Ptr2>
    bool operator!=(const rb_iterator<T, Ref2, Ptr2>& o) const {
        return node_ != o.node_;
    }

    Ref operator*()  const { return node_->value; }
    Ptr operator->() const { return &node_->value; }

    self& operator++() {
        if (node_->right != nil_) {
            // 右子树的最小节点
            node_ = rb_node<T>::minimum(node_->right, nil_);
        } else {
            // 向上找第一个"从左侧来"的祖先
            node_ptr p = node_->parent;
            while (p != header_ && node_ == p->right) {
                node_ = p;
                p = p->parent;
            }
            node_ = p;  // end() 时 node_ == header_
        }
        return *this;
    }
    self operator++(int) { auto t = *this; ++*this; return t; }

    self& operator--() {
        if (node_ == header_) {
            // end() 的 --：到最大节点
            node_ = header_->right;
        } else if (node_->left != nil_) {
            node_ = rb_node<T>::maximum(node_->left, nil_);
        } else {
            node_ptr p = node_->parent;
            while (p != header_ && node_ == p->left) {
                node_ = p;
                p = p->parent;
            }
            node_ = p;
        }
        return *this;
    }
    self operator--(int) { auto t = *this; --*this; return t; }
};

// ---------- rb_tree 容器 ----------
template <typename Key, typename Value, typename KeyOfValue,
          typename Compare = std::less<Key>,
          typename Alloc = Allocator<rb_node<Value>>>
class rb_tree {
public:
    using key_type    = Key;
    using value_type  = Value;
    using size_type   = std::size_t;
    using iterator    = rb_iterator<Value, Value&, Value*>;
    using const_iterator = rb_iterator<Value, const Value&, const Value*>;
    using node_ptr    = rb_node<Value>*;

    rb_tree() : size_(0) {
        nil_    = alloc_.allocate(1);
        nil_->color  = RBColor::Black;
        nil_->left   = nil_->right = nil_->parent = nil_;

        header_ = alloc_.allocate(1);
        header_->color  = RBColor::Red;
        header_->parent = nil_;   // parent = root（初始为 nil_）
        header_->left   = header_->right = header_;
    }

    rb_tree(const rb_tree& o) : rb_tree() {
        for (auto it = o.begin(); it != o.end(); ++it)
            insert_equal(*it);
    }
    rb_tree(rb_tree&& o) noexcept
        : nil_(o.nil_), header_(o.header_), size_(o.size_), comp_(o.comp_) {
        o.nil_ = o.header_ = nullptr; o.size_ = 0;
    }

    ~rb_tree() {
        if (!nil_) return;
        clear_node(root());
        alloc_.deallocate(nil_, 1);
        alloc_.deallocate(header_, 1);
    }

    // ==================== 基本信息 ====================
    bool      empty()    const { return size_ == 0; }
    size_type size()     const { return size_; }
    node_ptr  root()     const { return header_->parent; }
    iterator  begin()    { return {header_->left,  nil_, header_}; }
    iterator  end()      { return {header_,         nil_, header_}; }
    const_iterator begin()  const { return {header_->left,  nil_, header_}; }
    const_iterator end()    const { return {header_,         nil_, header_}; }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }

    // ==================== 插入 ====================

    // insert_equal：允许重复 key
    iterator insert_equal(const Value& v) {
        node_ptr p = header_, x = root();
        while (x != nil_) {
            p = x;
            x = comp_(kof_(v), kof_(x->value)) ? x->left : x->right;
        }
        return insert_at(x, p, v);
    }

    // insert_unique：不允许重复 key，返回 {iter, inserted}
    std::pair<iterator, bool> insert_unique(const Value& v) {
        node_ptr p = header_, x = root();
        bool left = true;
        while (x != nil_) {
            p = x;
            left = comp_(kof_(v), kof_(x->value));
            x = left ? x->left : x->right;
        }
        // 检查是否重复
        iterator j{p, nil_, header_};
        if (left) {
            if (j == begin()) {
                return {insert_at(x, p, v), true};
            }
            --j;
        }
        if (comp_(kof_(*j), kof_(v))) {
            return {insert_at(x, p, v), true};
        }
        return {j, false};  // 已存在
    }

    // ==================== 查找 ====================
    iterator find(const Key& k) {
        node_ptr y = header_, x = root();
        while (x != nil_) {
            if (!comp_(kof_(x->value), k)) { y = x; x = x->left; }
            else                            x = x->right;
        }
        iterator j{y, nil_, header_};
        return (j == end() || comp_(k, kof_(*j))) ? end() : j;
    }

    const_iterator find(const Key& k) const {
        node_ptr y = header_, x = root();
        while (x != nil_) {
            if (!comp_(kof_(x->value), k)) { y = x; x = x->left; }
            else                            x = x->right;
        }
        const_iterator j{y, nil_, header_};
        return (j == end() || comp_(k, kof_(*j))) ? end() : j;
    }

    iterator lower_bound(const Key& k) {
        node_ptr y = header_, x = root();
        while (x != nil_) {
            if (!comp_(kof_(x->value), k)) { y = x; x = x->left; }
            else                            x = x->right;
        }
        return {y, nil_, header_};
    }

    iterator upper_bound(const Key& k) {
        node_ptr y = header_, x = root();
        while (x != nil_) {
            if (comp_(k, kof_(x->value))) { y = x; x = x->left; }
            else                           x = x->right;
        }
        return {y, nil_, header_};
    }

    size_type count(const Key& k) const {
        size_type n = 0;
        for (auto it = const_cast<rb_tree*>(this)->lower_bound(k);
             it != end() && !comp_(k, kof_(*it)); ++it) ++n;
        return n;
    }

    // ==================== 删除 ====================
    void erase(iterator pos) {
        node_ptr z = pos.node_;
        --size_;
        node_ptr y = z, x, x_parent;
        RBColor y_orig_color = y->color;

        if (z->left == nil_) {
            x = z->right;
            x_parent = z->parent;
            transplant(z, z->right);
        } else if (z->right == nil_) {
            x = z->left;
            x_parent = z->parent;
            transplant(z, z->left);
        } else {
            // 找中序后继（右子树最小）
            y = rb_node<Value>::minimum(z->right, nil_);
            y_orig_color = y->color;
            x = y->right;
            if (y->parent == z) {
                x_parent = y;
            } else {
                x_parent = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        destroy(&z->value);
        alloc_.deallocate(z, 1);
        // 更新 header_->left / right
        if (size_ > 0) {
            header_->left  = rb_node<Value>::minimum(root(), nil_);
            header_->right = rb_node<Value>::maximum(root(), nil_);
        } else {
            header_->left = header_->right = header_;
        }
        if (y_orig_color == RBColor::Black)
            erase_fixup(x, x_parent);
    }

    size_type erase(const Key& k) {
        size_type n = 0;
        for (auto it = lower_bound(k); it != end() && !comp_(k, kof_(*it)); ) {
            auto nx = it; ++nx;
            erase(it);
            it = nx;
            ++n;
        }
        return n;
    }

    void clear() {
        clear_node(root());
        header_->parent = nil_;
        header_->left   = header_;
        header_->right  = header_;
        size_ = 0;
    }

private:
    node_ptr    nil_;
    node_ptr    header_;
    size_type   size_;
    Compare     comp_;
    KeyOfValue  kof_;
    Alloc       alloc_;

    node_ptr new_node(const Value& v) {
        node_ptr nd = alloc_.allocate(1);
        construct(&nd->value, v);
        nd->color  = RBColor::Red;
        nd->left   = nd->right = nd->parent = nil_;
        return nd;
    }

    void clear_node(node_ptr x) {
        if (x == nil_) return;
        clear_node(x->left);
        clear_node(x->right);
        destroy(&x->value);
        alloc_.deallocate(x, 1);
    }

    // 用 v 替换 u 在树中的位置
    void transplant(node_ptr u, node_ptr v) {
        if (u->parent == header_)     header_->parent = v;
        else if (u == u->parent->left) u->parent->left = v;
        else                           u->parent->right = v;
        v->parent = u->parent;
    }

    // ==================== 旋转 ====================
    void left_rotate(node_ptr x) {
        node_ptr y = x->right;
        x->right = y->left;
        if (y->left != nil_) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == header_)       header_->parent = y;
        else if (x == x->parent->left)  x->parent->left = y;
        else                            x->parent->right = y;
        y->left = x;
        x->parent = y;
    }

    void right_rotate(node_ptr x) {
        node_ptr y = x->left;
        x->left = y->right;
        if (y->right != nil_) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == header_)       header_->parent = y;
        else if (x == x->parent->right) x->parent->right = y;
        else                            x->parent->left = y;
        y->right = x;
        x->parent = y;
    }

    // ==================== 插入 ====================
    iterator insert_at(node_ptr x_pos, node_ptr parent, const Value& v) {
        node_ptr z = new_node(v);
        z->parent = parent;
        if (parent == header_) {
            header_->parent = z;
            header_->left   = z;
            header_->right  = z;
        } else if (comp_(kof_(v), kof_(parent->value))) {
            parent->left = z;
            if (parent == header_->left) header_->left = z;
        } else {
            parent->right = z;
            if (parent == header_->right) header_->right = z;
        }
        (void)x_pos;
        ++size_;
        insert_fixup(z);
        return {z, nil_, header_};
    }

    // 插入修复：维护红黑树性质
    void insert_fixup(node_ptr z) {
        while (z->parent != header_ && z->parent->color == RBColor::Red) {
            node_ptr p = z->parent;
            node_ptr g = p->parent;  // 祖父
            if (p == g->left) {
                node_ptr u = g->right;  // 叔父
                if (u->color == RBColor::Red) {
                    // Case 1：叔父红 → 重新染色，向上传播
                    p->color = RBColor::Black;
                    u->color = RBColor::Black;
                    g->color = RBColor::Red;
                    z = g;
                } else {
                    if (z == p->right) {
                        // Case 2：叔父黑，z 在右 → 左旋转为 Case 3
                        z = p;
                        left_rotate(z);
                        p = z->parent;
                        g = p->parent;
                    }
                    // Case 3：叔父黑，z 在左 → 右旋 + 染色
                    p->color = RBColor::Black;
                    g->color = RBColor::Red;
                    right_rotate(g);
                }
            } else {  // 镜像
                node_ptr u = g->left;
                if (u->color == RBColor::Red) {
                    p->color = RBColor::Black;
                    u->color = RBColor::Black;
                    g->color = RBColor::Red;
                    z = g;
                } else {
                    if (z == p->left) {
                        z = p;
                        right_rotate(z);
                        p = z->parent;
                        g = p->parent;
                    }
                    p->color = RBColor::Black;
                    g->color = RBColor::Red;
                    left_rotate(g);
                }
            }
        }
        root()->color = RBColor::Black;
    }

    // 删除修复：维护黑高相等
    void erase_fixup(node_ptr x, node_ptr x_parent) {
        while (x != root() && x->color == RBColor::Black) {
            if (x == x_parent->left) {
                node_ptr w = x_parent->right;  // 兄弟
                if (w->color == RBColor::Red) {
                    // Case 1 → 转为 Case 2/3/4
                    w->color = RBColor::Black;
                    x_parent->color = RBColor::Red;
                    left_rotate(x_parent);
                    w = x_parent->right;
                }
                if (w->left->color == RBColor::Black &&
                    w->right->color == RBColor::Black) {
                    // Case 2：兄弟的两个孩子都黑
                    w->color = RBColor::Red;
                    x = x_parent;
                    x_parent = x->parent;
                } else {
                    if (w->right->color == RBColor::Black) {
                        // Case 3：兄弟右黑左红 → 右旋转为 Case 4
                        w->left->color = RBColor::Black;
                        w->color = RBColor::Red;
                        right_rotate(w);
                        w = x_parent->right;
                    }
                    // Case 4：兄弟右红 → 左旋 + 染色，完成
                    w->color = x_parent->color;
                    x_parent->color = RBColor::Black;
                    w->right->color = RBColor::Black;
                    left_rotate(x_parent);
                    x = root();
                }
            } else {  // 镜像
                node_ptr w = x_parent->left;
                if (w->color == RBColor::Red) {
                    w->color = RBColor::Black;
                    x_parent->color = RBColor::Red;
                    right_rotate(x_parent);
                    w = x_parent->left;
                }
                if (w->right->color == RBColor::Black &&
                    w->left->color == RBColor::Black) {
                    w->color = RBColor::Red;
                    x = x_parent;
                    x_parent = x->parent;
                } else {
                    if (w->left->color == RBColor::Black) {
                        w->right->color = RBColor::Black;
                        w->color = RBColor::Red;
                        left_rotate(w);
                        w = x_parent->left;
                    }
                    w->color = x_parent->color;
                    x_parent->color = RBColor::Black;
                    w->left->color = RBColor::Black;
                    right_rotate(x_parent);
                    x = root();
                }
            }
        }
        x->color = RBColor::Black;
    }
};

} // namespace mystl
