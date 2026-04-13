# Phase 03 — 容器（Containers）

> 本阶段在 `namespace mystl` 中从零实现 8 类容器，覆盖顺序容器、适配器、堆结构、平衡树和哈希表。
> 所有容器均依赖 `phase01_allocator`（内存分配与对象构造）和 `phase02_iterator`（迭代器标签与工具函数）。

---

## 目录

1. [vector — 动态数组](#1-vector--动态数组)
2. [list — 环状双向链表](#2-list--环状双向链表)
3. [deque — 双端队列](#3-deque--双端队列)
4. [stack / queue — 容器适配器](#4-stack--queue--容器适配器)
5. [heap / priority_queue — 堆与优先队列](#5-heap--priority_queue--堆与优先队列)
6. [rb_tree — 红黑树](#6-rb_tree--红黑树)
7. [map / set — 有序关联容器](#7-map--set--有序关联容器)
8. [hashtable / hash_map / hash_set — 哈希容器](#8-hashtable--hash_map--hash_set--哈希容器)

---

## 1. vector — 动态数组

### 核心设计

`vector` 是最基础的顺序容器，用一块**连续内存**存放元素。其内部只靠 **3 根指针**描述全部状态：

```
内存布局：
┌──────────────────────────────────────────────────────┐
│  已用元素 [start_, finish_)  │  预留空间              │
└──────────────────────────────────────────────────────┘
^                              ^                        ^
start_                      finish_             end_of_storage_
```

| 指针 | 含义 |
|------|------|
| `start_` | 分配内存的起始地址 |
| `finish_` | 已构造元素的末尾（下一个空位） |
| `end_of_storage_` | 分配内存的末尾 |

- `size()` = `finish_ - start_`
- `capacity()` = `end_of_storage_ - start_`

因为迭代器直接用裸指针 `T*`，所有随机访问操作均为 O(1)。

### 关键数据成员

```cpp
// vector.h
template <typename T, typename Alloc = Allocator<T>>
class vector {
private:
    pointer start_;           // 内存起始
    pointer finish_;          // 有效元素末尾
    pointer end_of_storage_;  // 内存末尾
    Alloc   alloc_;
};
```

### 扩容策略：容量翻倍

`push_back` 在 `finish_ == end_of_storage_` 时触发扩容：

```cpp
// 扩容：容量翻倍（至少为 1）
void grow() {
    reallocate(std::max(capacity() * 2, size_type(1)));
}

void reallocate(size_type new_cap) {
    size_type old_size = size();
    pointer new_start = alloc_.allocate(new_cap);
    // 用移动语义搬移旧元素，避免不必要的拷贝
    for (size_type i = 0; i < old_size; ++i)
        construct(new_start + i, std::move(start_[i]));
    destroy_and_free();           // 析构旧元素 + 释放旧内存
    start_          = new_start;
    finish_         = start_ + old_size;
    end_of_storage_ = start_ + new_cap;
}
```

均摊分析：每次翻倍使得 `n` 次 `push_back` 的总搬移代价为 `O(n)`，均摊每次 O(1)。

### 移动语义

移动构造只做**指针接管**，原对象三指针全部置 `nullptr`，O(1) 完成：

```cpp
vector(vector&& other) noexcept
    : start_(other.start_), finish_(other.finish_),
      end_of_storage_(other.end_of_storage_) {
    other.start_ = other.finish_ = other.end_of_storage_ = nullptr;
}
```

### 中间插入

`emplace` 在任意位置插入时，先确保容量充足，然后用 `std::move_backward` 将后半段向右移一格，最后在目标位置原位构造：

```cpp
template <typename... Args>
iterator emplace(const_iterator cpos, Args&&... args) {
    size_type offset = cpos - start_;
    if (finish_ == end_of_storage_) grow();
    iterator pos = start_ + offset;
    if (pos != finish_) {
        construct(finish_, std::move(*(finish_ - 1)));
        std::move_backward(pos, finish_ - 1, finish_);
        destroy(pos);
    }
    construct(pos, std::forward<Args>(args)...);
    ++finish_;
    return pos;
}
```

### 要点小结

- **3 指针**描述全部状态，简洁高效
- 扩容时**容量翻倍**，均摊 O(1) `push_back`
- 扩容搬移元素时使用**移动语义**，避免深拷贝
- `iterator` 直接是 `T*`，随机访问零开销
- 中间插入/删除需搬移元素，最坏 O(n)

---

## 2. list — 环状双向链表

### 核心设计

`list` 使用**环状双向链表 + 一个哨兵节点**的经典设计：

```
哨兵节点 sentinel_ 既是 end()，也是链接环的闭合点：

sentinel_ <──> node1 <──> node2 <──> ... <──> nodeN <──> sentinel_
   (end)        (begin)                          (rbegin)
```

哨兵节点本身不存储数据，其 `prev` 指向最后一个元素，`next` 指向第一个元素。空链表时 `sentinel_->next == sentinel_->prev == sentinel_`，`begin() == end()` 自然成立，无需特判。

### 节点与迭代器

```cpp
// 链表节点
template <typename T>
struct list_node {
    list_node* prev;
    list_node* next;
    T          data;
};

// 迭代器：仅包一根节点指针
template <typename T, typename Ref, typename Ptr>
struct list_iterator {
    list_node<T>* node_;

    Ref operator*()  const { return node_->data; }
    self& operator++() { node_ = node_->next; return *this; }
    self& operator--() { node_ = node_->prev; return *this; }
};
```

迭代器类型为**双向迭代器**（`bidirectional_iterator_tag`），插入/删除不会使其他迭代器失效，因为操作只改指针，不搬移数据。

### splice：O(1) 区间转移

`splice` 是 `list` 最关键的低层原语，把 `[first, last)` 从 `other` 摘出并接到 `this` 的 `pos` 之前：

```cpp
void splice(const_iterator pos, list& other,
            const_iterator first, const_iterator last) {
    if (first == last) return;
    list_node<T>* f = first.node_;
    list_node<T>* l = last.node_->prev;
    // 从 other 摘出 [f, l]
    f->prev->next = last.node_;
    last.node_->prev = f->prev;
    // 插入到 this 的 pos 之前
    list_node<T>* p = pos.node_;
    f->prev    = p->prev;
    l->next    = p;
    p->prev->next = f;
    p->prev    = l;
}
```

整个过程只有 4 根指针修改，O(1)。

### sort：基于 splice 的归并排序

`list` 不能用 `std::sort`（需要随机访问迭代器），改用**链表原地归并排序**：

```cpp
void sort() {
    if (empty() || sentinel_->next->next == sentinel_) return;
    list left, right;
    size_type mid = size() / 2;
    auto it = begin();
    mystl::advance(it, static_cast<std::ptrdiff_t>(mid));
    right.splice(right.begin(), *this, it, end());
    left.splice(left.begin(), *this, begin(), end());
    left.sort();
    right.sort();
    merge(left, right);   // 归并时用 splice，不复制数据
}
```

归并阶段的 `merge` 通过反复调用 `splice` 将节点**转移**（而非复制）到 `*this`，整个过程零数据搬移。

### 要点小结

- **哨兵节点**消除边界特判，`begin()` = `sentinel_->next`，`end()` = `sentinel_`
- 插入/删除均为 O(1)，不使已有迭代器失效
- `splice` 是核心原语：O(1) 转移任意区间
- `sort` 用归并排序实现，借助 `splice` 实现零拷贝归并

---

## 3. deque — 双端队列

### 核心设计

`deque` 需要支持 O(1) 两端插入，但又希望支持随机访问。它的解法是**分段连续存储**：

```
map_（指针数组，即"中央控制器"）：
┌────┬────┬────┬────┬────┬────┐
│    │buf0│buf1│buf2│buf3│    │  ← map_ 中的槽位，居中排列
└────┴────┴────┴────┴────┴────┘
       ↓    ↓    ↓    ↓
     [####][####][####][####]    ← 每个 buffer，固定大小 BUF 个元素

start_ 迭代器指向 buf0 的某个位置（队首）
finish_ 迭代器指向 buf3 的某个位置（队尾）
```

Buffer 大小计算：
```cpp
template <typename T>
constexpr std::size_t deque_buf_size() {
    return sizeof(T) < 256 ? (512 / sizeof(T)) : std::size_t(1);
}
```
当 `sizeof(T) < 256` 时，每个 buffer 约 512 字节；元素很大时退化为每 buffer 1 个元素。

### 迭代器：4 字段设计

deque 迭代器维护 4 根指针，是其复杂性的核心：

```cpp
template <typename T, typename Ref, typename Ptr>
struct deque_iterator {
    static constexpr std::size_t BUF = deque_buf_size<T>();

    T*  cur;    // 当前元素位置
    T*  first;  // 当前 buffer 起始
    T*  last;   // 当前 buffer 末尾（past-the-end）
    T** node;   // 指向 map_ 中对应的槽位
};
```

跨 buffer 的 `++` 操作：

```cpp
self& operator++() {
    ++cur;
    if (cur == last) {        // 走到 buffer 末尾
        set_node(node + 1);   // 跳到 map_ 下一槽位
        cur = first;          // 指向新 buffer 起始
    }
    return *this;
}
```

跨 buffer 的随机跳转（`operator+=`）：

```cpp
self& operator+=(std::ptrdiff_t n) {
    std::ptrdiff_t offset = n + (cur - first);  // 相对当前 buffer 起始的偏移
    if (offset >= 0 && offset < std::ptrdiff_t(BUF)) {
        cur += n;   // 还在当前 buffer，直接移动
    } else {
        // 计算跨越的 buffer 数
        std::ptrdiff_t node_offset =
            offset > 0
            ? offset / std::ptrdiff_t(BUF)
            : -std::ptrdiff_t((-offset - 1) / BUF) - 1;
        set_node(node + node_offset);
        cur = first + (offset - node_offset * std::ptrdiff_t(BUF));
    }
    return *this;
}
```

两个迭代器的距离计算：

```cpp
std::ptrdiff_t operator-(const self& o) const {
    return std::ptrdiff_t(BUF) * (node - o.node - 1)
           + (cur - first)       // this 在本 buffer 内的偏移
           + (o.last - o.cur);   // o 在本 buffer 内的剩余量
}
```

### 初始化：居中排列

`init_map` 在 map 中**居中**分配 buffer，使得两端都有扩展空间：

```cpp
void init_map(size_type n_elems) {
    constexpr size_type INITIAL_MAP_SIZE = 8;
    size_type n_bufs = n_elems / BUF + 1;
    map_size_ = std::max(INITIAL_MAP_SIZE, n_bufs + 2);
    map_ = map_alloc_.allocate(map_size_);
    // 居中排列，两端各留余量
    T** nstart  = map_ + (map_size_ - n_bufs) / 2;
    T** nfinish = nstart + n_bufs;
    for (T** cur = nstart; cur < nfinish; ++cur)
        *cur = alloc_buf();
    start_.set_node(nstart);
    finish_.set_node(nfinish - 1);
    start_.cur  = start_.first;
    finish_.cur = finish_.first + n_elems % BUF;
}
```

### map 扩容策略

当 map 两端的槽位耗尽时，`reallocate_map` 被触发：
- 若 map 本身仍有足够空间，只需**重新居中**（`memmove` 指针）；
- 否则分配更大的 map，将原有 buffer 指针拷贝过去，不需要搬移任何元素数据。

### 要点小结

- **中央 map + 固定大小 buffer**，两端 push 均为 O(1) 均摊
- 迭代器维护 `cur/first/last/node` 4 个字段，支持跨 buffer 的随机访问
- `operator+=` 通过整除计算跨越 buffer 数，O(1) 随机跳转
- 扩容只重新分配 map（指针数组），不搬移元素

---

## 4. stack / queue — 容器适配器

### 核心设计

`stack` 和 `queue` 是**适配器（Adapter）**：它们不自己管理内存，而是持有一个底层容器，通过限制接口来提供不同的语义。

```cpp
// stack 包装 deque，对外只暴露 LIFO 操作
template <typename T, typename Container = mystl::deque<T>>
class stack {
private:
    Container c_;   // 底层容器
};

// queue 包装 deque，对外只暴露 FIFO 操作
template <typename T, typename Container = mystl::deque<T>>
class queue {
private:
    Container c_;
};
```

### stack：后进先出（LIFO）

```cpp
reference top()       { return c_.back(); }   // 访问栈顶
void push(const T& v) { c_.push_back(v); }    // 入栈 = deque::push_back
void pop()            { c_.pop_back(); }       // 出栈 = deque::pop_back
```

### queue：先进先出（FIFO）

```cpp
reference front()     { return c_.front(); }  // 队首
reference back()      { return c_.back(); }   // 队尾
void push(const T& v) { c_.push_back(v); }    // 入队 = deque::push_back
void pop()            { c_.pop_front(); }     // 出队 = deque::pop_front
```

`deque` 支持 O(1) 的 `push_front`/`push_back`/`pop_front`/`pop_back`，这让它天然适合同时作 `stack` 和 `queue` 的底层。

### 替换底层容器

适配器对底层容器的类型是模板参数，可以自由替换：

```cpp
// 用 mystl::list 作为底层
mystl::stack<int, mystl::list<int>> s;

// 用 mystl::vector 作为 stack 底层（不支持 queue，因为没有 pop_front）
mystl::stack<int, mystl::vector<int>> s2;
```

### 要点小结

- 适配器模式：**组合而非继承**，通过 `c_` 复用底层容器的实现
- `stack`: `top` → `back`，`push` → `push_back`，`pop` → `pop_back`
- `queue`: `front` → `front`，`push` → `push_back`，`pop` → `pop_front`
- 底层容器类型是模板参数，可以自由替换（鸭子类型）

---

## 5. heap / priority_queue — 堆与优先队列

### 堆的基本性质

`heap` 是一棵**完全二叉树**，以数组表示，下标从 0 开始：
- 父节点 `i` 的左孩子：`2i + 1`
- 父节点 `i` 的右孩子：`2i + 2`
- 孩子节点 `i` 的父节点：`(i - 1) / 2`

**大顶堆**性质：`parent >= children`，即堆顶（`[0]`）始终是最大元素。

### 两个核心操作

**sift_up（上浮）**：新元素加到末尾后，反复与父节点比较，若更大则上移：

```cpp
template <typename RandomIt, typename Distance, typename T, typename Compare>
void sift_up(RandomIt first, Distance hole, Distance top, T value, Compare comp) {
    Distance parent = (hole - 1) / 2;
    while (hole > top && comp(*(first + parent), value)) {
        *(first + hole) = std::move(*(first + parent));  // 父节点下移
        hole   = parent;
        parent = (hole - 1) / 2;
    }
    *(first + hole) = std::move(value);  // 落定
}
```

**sift_down（下沉）**：堆顶被取出后，末尾元素移到堆顶，反复与较大孩子比较，若更小则下移：

```cpp
template <typename RandomIt, typename Distance, typename T, typename Compare>
void sift_down(RandomIt first, Distance hole, Distance len, T value, Compare comp) {
    Distance child = 2 * hole + 2;   // 右孩子
    while (child < len) {
        if (comp(*(first + child), *(first + (child - 1))))
            --child;                 // 选较大的孩子
        *(first + hole) = std::move(*(first + child));
        hole  = child;
        child = 2 * hole + 2;
    }
    if (child == len) {              // 只有左孩子
        *(first + hole) = std::move(*(first + (child - 1)));
        hole = child - 1;
    }
    sift_up(first, hole, Distance(0), std::move(value), comp);
}
```

注意：`sift_down` 最后调用 `sift_up` 是因为下沉到叶子附近时，末尾值可能并不小于其路径上的某个元素，需要做一次向上修正。

### 堆算法接口

```cpp
// 将 *(last-1) 加入 [first, last-1) 已有的堆
push_heap(first, last);
push_heap(first, last, comp);

// 将最大元素移到末尾，[first, last-1) 重建为堆
pop_heap(first, last);

// 将任意区间整理为堆（从最后一个非叶节点 (n-2)/2 开始逐个 sift_down）
make_heap(first, last);

// 堆排序：反复 pop_heap，结果为升序
sort_heap(first, last);

// 检查区间是否满足堆性质
is_heap(first, last);
```

`make_heap` 的时间复杂度为 O(n)（不是 O(n log n)）；`sort_heap` 为 O(n log n)。

### priority_queue

`priority_queue` 封装 `mystl::vector` 和堆算法，提供"永远能取到最大值"的队列：

```cpp
template <typename T,
          typename Container = mystl::vector<T>,
          typename Compare   = std::less<T>>
class priority_queue {
private:
    Container c_;    // 底层存储（默认 mystl::vector）
    Compare   comp_; // 比较器（默认 less，即大顶堆）
};
```

`push` 和 `pop` 的实现：

```cpp
void push(const T& v) {
    c_.push_back(v);
    // 新元素加到末尾后上浮
    Distance hole  = static_cast<Distance>(c_.size()) - 1;
    auto     value = std::move(c_[hole]);
    sift_up(c_.begin(), hole, Distance(0), std::move(value), comp_);
}

void pop() {
    // 将末尾元素与堆顶交换，堆顶移到末尾，对新堆顶做下沉
    auto value  = std::move(c_.back());
    c_.back()   = std::move(c_.front());
    c_.pop_back();
    if (!c_.empty())
        sift_down(c_.begin(), Distance(0), len - 1, std::move(value), comp_);
}
```

### 要点小结

- 大顶堆：`parent >= children`，堆顶始终为最大值
- `sift_up` O(log n)，`sift_down` O(log n)，`make_heap` O(n)
- `push_heap` = 追加 + `sift_up`；`pop_heap` = 交换堆顶与末尾 + `sift_down`
- `priority_queue` 是对 `mystl::vector` + 堆算法的薄封装，`top()` = O(1)

---

## 6. rb_tree — 红黑树

### 五条性质

红黑树通过对节点着色（红/黑）来保证**近似平衡**，使所有操作的最坏时间为 O(log n)：

1. 每个节点是**红色**或**黑色**
2. **根节点**是黑色
3. 所有**叶节点**（NIL/哨兵）是黑色
4. 红色节点的两个子节点都是**黑色**（不存在连续红节点）
5. 从任意节点到其后代叶节点的所有路径，包含**相同数量的黑色节点**（黑高相等）

### 哨兵双节点设计

本实现使用**两个特殊节点**：

```
nil_（黑色叶哨兵）：所有"空子树"都指向 nil_，而非 nullptr
header_（红色头节点）：
    header_->parent = 根节点
    header_->left   = 树中最小节点（begin() 的起点）
    header_->right  = 树中最大节点（rbegin() 的起点）
```

这样 `begin()` = `{header_->left, ...}`，`end()` = `{header_, ...}`，中序遍历可以自然地从最小到 `header_` 结束。

```cpp
rb_tree() : size_(0) {
    nil_          = alloc_.allocate(1);
    nil_->color   = RBColor::Black;
    nil_->left    = nil_->right = nil_->parent = nil_;

    header_         = alloc_.allocate(1);
    header_->color  = RBColor::Red;
    header_->parent = nil_;        // 初始：根为 nil_（空树）
    header_->left   = header_->right = header_;
}
```

### 迭代器：中序遍历

迭代器需要同时持有 `nil_` 和 `header_` 才能正确遍历，以及支持跨类型（`iterator` vs `const_iterator`）的比较：

```cpp
template <typename T, typename Ref, typename Ptr>
struct rb_iterator {
    node_ptr node_;    // 当前节点
    node_ptr nil_;     // 叶哨兵
    node_ptr header_;  // 头哨兵

    // 跨类型比较：iterator == const_iterator
    template <typename Ref2, typename Ptr2>
    bool operator==(const rb_iterator<T, Ref2, Ptr2>& o) const {
        return node_ == o.node_;
    }

    self& operator++() {
        if (node_->right != nil_) {
            // 有右子树：找右子树最小节点
            node_ = rb_node<T>::minimum(node_->right, nil_);
        } else {
            // 无右子树：向上找"从左侧来"的第一个祖先
            node_ptr p = node_->parent;
            while (p != header_ && node_ == p->right) {
                node_ = p; p = p->parent;
            }
            node_ = p;  // 走到 header_ 即为 end()
        }
        return *this;
    }
};
```

### 插入修复：3 种情况

新节点初始为**红色**（不影响黑高），但可能与红色父节点发生冲突（违反性质 4）。`insert_fixup` 按叔父节点颜色和 z 的位置分 3 种情况处理（父节点在祖父左侧，镜像同理）：

```
Case 1：叔父红
    → 父、叔变黑，祖父变红，z 上移到祖父，继续向上修复
    （黑高不变，但祖父变红可能新产生冲突）

Case 2：叔父黑，z 是父的右孩子
    → 以父为轴左旋，将 z 转化为 Case 3

Case 3：叔父黑，z 是父的左孩子
    → 父变黑，祖父变红，以祖父为轴右旋
    （修复完成，黑高守恒）
```

```cpp
void insert_fixup(node_ptr z) {
    while (z->parent != header_ && z->parent->color == RBColor::Red) {
        node_ptr p = z->parent, g = p->parent;
        if (p == g->left) {
            node_ptr u = g->right;  // 叔父
            if (u->color == RBColor::Red) {            // Case 1
                p->color = u->color = RBColor::Black;
                g->color = RBColor::Red;
                z = g;
            } else {
                if (z == p->right) {                   // Case 2 → Case 3
                    z = p; left_rotate(z);
                    p = z->parent; g = p->parent;
                }
                p->color = RBColor::Black;             // Case 3
                g->color = RBColor::Red;
                right_rotate(g);
            }
        } else { /* 镜像 */ }
    }
    root()->color = RBColor::Black;  // 保证根为黑
}
```

### 删除修复：4 种情况

删除一个黑色节点会破坏黑高，`erase_fixup` 通过"双重黑"概念（x 节点额外携带一个黑色）来追踪缺失的黑色，分 4 种情况恢复（x 在父节点左侧，镜像同理）：

```
Case 1：兄弟 w 红
    → 兄弟变黑，父变红，以父为轴左旋；转化为 Case 2/3/4

Case 2：兄弟 w 黑，w 的两孩子均黑
    → 兄弟变红，双重黑上移到父节点，继续修复

Case 3：兄弟 w 黑，w 右孩子黑，左孩子红
    → 兄弟左孩子变黑，兄弟变红，以兄弟为轴右旋；转化为 Case 4

Case 4：兄弟 w 黑，w 右孩子红
    → 兄弟继承父色，父和兄弟右孩子变黑，以父为轴左旋；修复完成
```

### insert_unique / insert_equal

```cpp
// insert_unique：key 不重复（用于 set / map）
std::pair<iterator, bool> insert_unique(const Value& v);

// insert_equal：允许重复 key（用于 multiset / multimap）
iterator insert_equal(const Value& v);
```

`KeyOfValue` 模板参数允许 rb_tree 从 value 中提取 key，使同一棵树可以同时支持 `set`（key == value）和 `map`（key == value.first）。

### 要点小结

- `nil_` 统一代替所有 `nullptr`，消除边界判断
- `header_` 的 `left`/`right` 始终追踪最小/最大节点，使 `begin()`/`rbegin()` 为 O(1)
- 插入修复 3 种 case，最多 O(log n) 次旋转
- 删除修复 4 种 case，最多 O(log n) 次旋转
- 中序遍历 = 有序序列，所有操作最坏 O(log n)

---

## 7. map / set — 有序关联容器

### 核心设计

`map`、`set`、`multimap`、`multiset` 全部是 `rb_tree` 的薄封装，差别只在两个维度：

| 容器 | 是否允许重复 | `value_type` | `KeyOfValue` |
|------|-------------|--------------|-------------|
| `set` | 否 | `Key` | `Identity<Key>` |
| `multiset` | 是 | `Key` | `Identity<Key>` |
| `map` | 否 | `pair<const Key, T>` | `SelectFirst<pair<const Key, T>>` |
| `multimap` | 是 | `pair<const Key, T>` | `SelectFirst<pair<const Key, T>>` |

`Identity` 和 `SelectFirst` 定义在 `key_extractors.h`：

```cpp
// set：value 就是 key
template <typename T>
struct Identity {
    const T& operator()(const T& v) const { return v; }
};

// map：pair 的 first 是 key
template <typename Pair>
struct SelectFirst {
    const typename Pair::first_type& operator()(const Pair& p) const {
        return p.first;
    }
};
```

### set 与 map 的树类型

```cpp
// set 的 rb_tree：Key == Value，用 Identity 提取 key
using tree_type = rb_tree<Key, Key, Identity<Key>, Compare, Alloc>;

// map 的 rb_tree：Value 是 pair，用 SelectFirst 提取 key
using PairType  = std::pair<const Key, T>;
using tree_type = rb_tree<Key, PairType, SelectFirst<PairType>, Compare, Alloc>;
```

`set` 的迭代器固定为 `const_iterator`（不允许通过迭代器修改 key），`map` 的迭代器为 `iterator`（允许修改 mapped_type `T`，但 key 是 `const`）。

### map::operator[]

`operator[]` 是 `map` 特有的便捷接口：找到 key 则返回引用，找不到则**插入默认值后返回引用**：

```cpp
T& operator[](const Key& k) {
    auto it = tree_.lower_bound(k);
    // lower_bound 找到第一个 >= k 的位置
    if (it == tree_.end() || Compare{}(k, it->first)) {
        // 不存在：插入 {k, T{}} 并返回新值的引用
        it = tree_.insert_unique({k, T{}}).first;
    }
    return const_cast<T&>(it->second);
}
```

这个实现的关键在于用 `lower_bound` 而非 `find`，这样只做一次树遍历（而非先 find 再 insert）。

### 使用示例

```cpp
// set
mystl::set<int> s = {3, 1, 4, 1, 5};  // 自动去重并排序
for (int x : s) { /* 1, 3, 4, 5 */ }

// map
mystl::map<std::string, int> freq;
freq["hello"]++;   // 不存在则插入 {"hello", 0}，然后 +1
freq["world"]++;

auto it = freq.find("hello");   // O(log n)
```

### 要点小结

- 4 个容器共用同一棵 `rb_tree`，代码极简
- 通过 `KeyOfValue` 策略类（`Identity`/`SelectFirst`）统一处理 key 提取
- `set` 迭代器为 `const`，`map` 允许修改 mapped value
- `map::operator[]` 利用 `lower_bound` 实现"找不到则插入"，仅一次树查找

---

## 8. hashtable / hash_map / hash_set — 哈希容器

### 核心设计

`hashtable` 采用**开链法（链地址法）**解决哈希冲突：桶数组的每个槽位是一条单向链表的头指针。

```
buckets_ 数组（大小为素数）：

index 0: nullptr
index 1: node("banana") -> node("apple") -> nullptr
index 2: node("cherry") -> nullptr
index 3: nullptr
...
```

### 节点结构

```cpp
template <typename T>
struct ht_node {
    ht_node* next;   // 链表指针
    T        value;  // 存储值
};
```

### 素数桶大小

使用预定义的素数表，每次扩容选取大于目标大小的最小素数。素数作为桶数能减少哈希碰撞的聚集效应：

```cpp
static const std::size_t prime_list[] = {
    53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
    49157, 98317, 196613, 393241, 786433, ...
};

inline std::size_t next_prime(std::size_t n) {
    for (std::size_t i = 0; i < num_primes; ++i)
        if (prime_list[i] >= n) return prime_list[i];
    return prime_list[num_primes - 1];
}
```

### 负载因子与 rehash

当 `size_ >= buckets_.size()`（即负载因子 > 1.0）时触发重哈希：

```cpp
void rehash_if_needed() {
    if (size_ >= buckets_.size()) {
        std::size_t new_n = next_prime(buckets_.size() * 2);
        std::vector<ht_node<T>*> new_buckets(new_n, nullptr);
        for (auto* nd : buckets_) {
            while (nd) {
                auto* nx  = nd->next;
                std::size_t idx = hash_(kof_(nd->value)) % new_n;
                // 直接复用旧节点，不分配新内存
                nd->next          = new_buckets[idx];
                new_buckets[idx]  = nd;
                nd = nx;
            }
        }
        buckets_.swap(new_buckets);  // O(1) 交换
    }
}
```

重哈希时**复用旧节点**（只改 `next` 指针），不分配新内存，效率高。

### 迭代器：跨桶遍历

哈希表迭代器是**前向迭代器**，遍历时需要在当前链表走完后找下一个非空桶：

```cpp
self& operator++() {
    node_ptr old = cur_;
    cur_ = cur_->next;       // 先在链表内前进
    if (!cur_) {
        // 链表走完，找下一个非空桶
        std::size_t idx = ht_->bucket_index(old->value) + 1;
        while (idx < ht_->buckets_.size() && !ht_->buckets_[idx])
            ++idx;
        cur_ = (idx < ht_->buckets_.size()) ? ht_->buckets_[idx] : nullptr;
    }
    return *this;
}
```

### hash_set 与 hash_map

两者都是对 `hashtable` 的薄封装，通过 `KeyOf` 策略区分：

```cpp
// hash_set：value == key，用 Identity
template <typename T, typename Hash = std::hash<T>, ...>
class hash_set {
    using HT = hashtable<T, Hash, KeyEq, Identity<T>>;
    HT ht_;
};

// hash_map：value 是 pair，用 SelectFirst
template <typename Key, typename T, typename Hash = std::hash<Key>, ...>
class hash_map {
    using PairType = std::pair<const Key, T>;
    using HT = hashtable<PairType, Hash, KeyEq, SelectFirst<PairType>>;
    HT ht_;
};
```

`hash_map::operator[]` 利用 `insert_unique` 的"不存在则插入"语义：

```cpp
T& operator[](const Key& k) {
    auto [it, ok] = ht_.insert_unique({k, T{}});
    return const_cast<T&>(it->second);
}
```

### 与 rb_tree 的对比

| 特性 | rb_tree（map/set） | hashtable（hash_map/hash_set） |
|------|-------------------|-------------------------------|
| 时间复杂度（均摊） | O(log n) | O(1) |
| 元素顺序 | 有序（中序遍历） | 无序 |
| 迭代器类型 | 双向迭代器 | 前向迭代器 |
| 最坏情况 | O(log n) | O(n)（哈希全碰撞） |
| 内存局部性 | 差（指针链接） | 较好（数组 + 短链表） |

### 要点小结

- 开链法：每个桶是一条单向链表，哈希冲突时追加到链表头（O(1)）
- 桶数始终为**素数**，减少规律性碰撞
- 负载因子 > 1.0 时触发 rehash，复用旧节点（不重新分配）
- 迭代器持有 `ht_` 指针用于跨桶遍历
- `hash_set`/`hash_map` 通过与 `set`/`map` 相同的 `KeyOf` 策略统一实现

---

## 文件总览

```
phase03_containers/
├── vector.h                 # 动态数组（3 指针 + 翻倍扩容）
├── list.h                   # 环状双向链表（哨兵 + splice）
├── deque.h                  # 双端队列（中央 map + 固定 buffer）
├── stack_queue.h            # 容器适配器（stack / queue）
├── heap_priority_queue.h    # 堆算法 + priority_queue
├── rb_tree.h                # 红黑树（nil_ + header_ 哨兵设计）
├── key_extractors.h         # Identity / SelectFirst
├── map_set.h                # map / set / multimap / multiset
└── hashtable.h              # 开链哈希表 + hash_set / hash_map
```

## 依赖关系

```
phase01_allocator  ←──  phase02_iterator  ←──  phase03_containers
    alloc.h                iterator.h              vector.h
    construct.h                                    list.h
    uninitialized.h                                deque.h
                                                   ...
```

所有容器的内存分配均通过 `Allocator<T>`（`phase01`）完成，迭代器标签和工具函数（`advance`、`distance`、`next`）来自 `phase02`。

---

## 九、复杂度分析表

下表汇总全部 8 类容器的核心操作时间复杂度：

| 容器 | 操作 | 时间复杂度 | 备注 |
|------|------|-----------|------|
| `vector` | `push_back`（均摊） | O(1) | 容量翻倍策略 |
| `vector` | `push_back`（最坏） | O(n) | 触发扩容时搬移所有元素 |
| `vector` | `insert` 中间 | O(n) | 需要向右搬移后半段 |
| `vector` | `erase` 中间 | O(n) | 需要向左搬移后半段 |
| `vector` | `operator[]` | O(1) | 裸指针偏移 |
| `vector` | `find`（线性） | O(n) | 无序，只能顺序扫描 |
| `list` | `push_front` | O(1) | 修改哨兵前驱/后继指针 |
| `list` | `push_back` | O(1) | 同上 |
| `list` | `insert`（已有迭代器） | O(1) | 只改 4 根指针 |
| `list` | `erase`（已有迭代器） | O(1) | 只改 2 根指针 + 析构 |
| `list` | `sort` | O(n log n) | 链表归并排序 |
| `list` | `splice` | O(1) | 区间转移，不拷贝数据 |
| `deque` | `push_front` | O(1) 均摊 | 偶发 map 扩容 |
| `deque` | `push_back` | O(1) 均摊 | 偶发 map 扩容 |
| `deque` | `operator[]` | O(1) | 整除计算 buffer 下标 |
| `deque` | `insert` 中间 | O(n) | 需搬移前半段或后半段 |
| `stack` / `queue` | `push` | O(1) 均摊 | 委托底层 `deque::push_back` |
| `stack` / `queue` | `pop` | O(1) | 委托底层 `pop_back`/`pop_front` |
| `stack` / `queue` | `top` / `front` | O(1) | 委托底层 `back`/`front` |
| `heap` | `make_heap` | O(n) | 从最后一个非叶节点逐个 `sift_down` |
| `heap` | `push_heap` | O(log n) | 追加 + `sift_up` |
| `heap` | `pop_heap` | O(log n) | 交换堆顶与末尾 + `sift_down` |
| `heap` | `sort_heap` | O(n log n) | 反复 `pop_heap` |
| `heap` | `is_heap` | O(n) | 逐对检查父子关系 |
| `rb_tree` / `map` / `set` | `insert` | O(log n) | 最多 O(log n) 次旋转修复 |
| `rb_tree` / `map` / `set` | `erase` | O(log n) | 查找 + 最多 O(log n) 次旋转 |
| `rb_tree` / `map` / `set` | `find` | O(log n) | 树高有界 2log(n+1) |
| `rb_tree` / `map` / `set` | `lower_bound` | O(log n) | 与 `find` 同路径 |
| `rb_tree` / `map` / `set` | `count` | O(log n + k) | k 为相等元素数 |
| `hashtable` / `hash_map` | `insert`（均摊） | O(1) | 不触发 rehash |
| `hashtable` / `hash_map` | `insert`（最坏，rehash） | O(n) | 重新分桶，复用旧节点 |
| `hashtable` / `hash_map` | `find`（均摊） | O(1) | 负载因子低时链表极短 |
| `hashtable` / `hash_map` | `find`（最坏） | O(n) | 哈希全碰撞退化为链表 |
| `hashtable` / `hash_map` | `erase` | O(1) 均摊 | 定位桶 O(1) + 链表遍历 |

---

## 十、代码走读：rb_tree 插入修复（3 种 case 图解）

### 前置说明

新节点 `z` 初始着**红色**，不影响黑高，但可能与红色父节点冲突（违反性质 4：红节点的孩子必须是黑色）。`insert_fixup` 从 `z` 开始向上修复，直到冲突消除或到达根节点。

**关键循环条件**：`z->parent != header_ && z->parent->color == Red`

- 必须同时检查 `z->parent != header_`：因为 `header_->color == Red`，若只判断父节点颜色，当 `z` 已经是根节点的子节点且 `z->parent == header_` 时，循环会永远为 `true`，造成**无限循环**。
- `z->parent != header_` 是提前终止条件，确保修复不越过根节点。

---

### Case 1：叔父节点为红色 → 重新着色，z 上移

```
        G(B)                G(R)  ← z 移到这里继续修复
       / \                 / \
     P(R) U(R)   →      P(B) U(B)
     /                  /
   z(R)               z(R)
```

操作：将父 P 和叔父 U 都变黑，祖父 G 变红，`z` 上移到 G 继续循环。
效果：本层黑高不变（G 变红但其父到 G 的路径黑数不变），冲突可能向上传播。

---

### Case 2：叔父节点为黑色，z 是父的**右孩子** → 左旋 P，转化为 Case 3

```
      G(B)              G(B)
     / \                / \
   P(R) U(B)  →      z(R) U(B)
     \                /
     z(R)           P(R)
                    ← 转为 Case 3
```

操作：以 P 为轴**左旋**，`z` 指针上移到原来的 P（旋转后 P 变成 z 的左孩子）。
效果：树形从"右折"变成"左直"，进入 Case 3 处理。

---

### Case 3：叔父节点为黑色，z 是父的**左孩子** → 右旋 G + 重新着色

```
     G(B)             P(B)
    / \               / \
  P(R) U(B)  →     z(R) G(R)
  /                       \
z(R)                      U(B)
```

操作：P 变黑，G 变红，以 G 为轴**右旋**。
效果：P 取代 G 的位置，P 为黑使得黑高守恒，冲突消除，循环终止。

---

### 关键代码回顾

```cpp
void insert_fixup(node_ptr z) {
    // 两个前置条件缺一不可：
    //   1. z->parent != header_  防止越过根节点（header_->color == Red）
    //   2. z->parent->color == Red  父节点为红才存在冲突
    while (z->parent != header_ && z->parent->color == RBColor::Red) {
        node_ptr p = z->parent, g = p->parent;
        if (p == g->left) {
            node_ptr u = g->right;
            if (u->color == RBColor::Red) {        // Case 1
                p->color = u->color = RBColor::Black;
                g->color = RBColor::Red;
                z = g;                              // z 上移，继续修复
            } else {
                if (z == p->right) {               // Case 2 → 转化为 Case 3
                    z = p; left_rotate(z);
                    p = z->parent; g = p->parent;
                }
                p->color = RBColor::Black;          // Case 3
                g->color = RBColor::Red;
                right_rotate(g);
            }
        } else { /* 镜像：p == g->right，方向对调 */ }
    }
    root()->color = RBColor::Black;  // 最终确保根为黑
}
```

---

## 十一、实现难点 & 踩坑记录

### 1. deque 跨 buffer 的迭代器算术：负偏移的 ceiling division

`operator+=` 中，当偏移量 `offset` 为负时，不能直接用 `offset / BUF` 计算跨越的 buffer 数，因为 C++ 整数除法向零截断，`-1 / BUF == 0`，导致方向错误（应当向前跨一个 buffer，却判断为留在原 buffer）。

正确做法是**向负无穷取整**（ceiling division for negative）：

```cpp
std::ptrdiff_t node_offset =
    offset > 0
    ? offset / std::ptrdiff_t(BUF)                          // 正偏移：直接整除
    : -std::ptrdiff_t((-offset - 1) / BUF) - 1;            // 负偏移：先取绝对值再修正
```

以 `offset = -1, BUF = 8` 为例：`-(-(-1)-1)/8 - 1 = -(0/8) - 1 = -1`，正确跨到前一个 buffer。

---

### 2. heap ADL 歧义：`mystl::make_heap` 与 `std::make_heap` 冲突

当 `priority_queue` 的底层容器是 `std::vector<T>` 时，调用 `mystl::make_heap(v.begin(), v.end(), comp)` 会触发 **ADL（Argument-Dependent Lookup）**：编译器在 `std::vector` 的迭代器所属命名空间（`std`）里找到 `std::make_heap`，与 `mystl::make_heap` 产生歧义，导致编译错误。

解决方案有两种：

```cpp
// 方案 A：默认容器改用 mystl::vector（ADL 只找 mystl 命名空间）
template <typename T,
          typename Container = mystl::vector<T>,   // ← 改这里
          typename Compare   = std::less<T>>
class priority_queue { ... };

// 方案 B：绕过堆算法接口，直接调用内部函数
sift_up(c_.begin(), hole, Distance(0), std::move(value), comp_);
```

---

### 3. rb_tree insert_fixup 无限循环：`header_->color == Red`

`header_` 是一个**红色**哨兵节点（设计如此，方便 `end()` 的实现）。若循环条件只写 `z->parent->color == Red`，当 `z` 上移到根节点后，`z->parent == header_`，而 `header_->color == Red`，条件仍然为 `true`，循环永不终止。

```cpp
// 错误写法（无限循环）：
while (z->parent->color == RBColor::Red) { ... }

// 正确写法：
while (z->parent != header_ && z->parent->color == RBColor::Red) { ... }
```

`z->parent != header_` 必须放在 `&&` 的**左侧**，利用短路求值，确保在越过根节点前提前退出。

---

### 4. cross-type iterator 比较：`iterator != const_iterator` 编译失败

`rb_tree::iterator` 和 `rb_tree::const_iterator` 是同一模板 `rb_iterator<T, Ref, Ptr>` 的两个**不同实例化**，默认生成的 `operator==` 只接受完全相同的类型，导致 `it == cit` 编译失败。

```cpp
// 错误：两个不同类型，无法直接比较
rb_tree::iterator it = ...;
rb_tree::const_iterator cit = ...;
if (it == cit) { ... }   // 编译错误
```

解决方案：在 `rb_iterator` 中加入**模板比较运算符**：

```cpp
template <typename T, typename Ref, typename Ptr>
struct rb_iterator {
    // 允许与任意 Ref2/Ptr2 实例化的 rb_iterator 比较
    template <typename Ref2, typename Ptr2>
    bool operator==(const rb_iterator<T, Ref2, Ptr2>& o) const {
        return node_ == o.node_;
    }
    template <typename Ref2, typename Ptr2>
    bool operator!=(const rb_iterator<T, Ref2, Ptr2>& o) const {
        return node_ != o.node_;
    }
};
```

---

### 5. `map::operator[]` 无法调用 `tree_.key_comp()`

`rb_tree` 内部存储了比较器 `Compare`，但若未对外暴露 `key_comp()` 接口，`map::operator[]` 就无法从树对象获取比较器。

```cpp
// 编译错误（rb_tree 未提供 key_comp()）：
if (tree_.key_comp()(k, it->first)) { ... }
```

解决方案：直接用 `Compare{}` 默认构造比较器（因为 `Compare` 是无状态函数对象）：

```cpp
T& operator[](const Key& k) {
    auto it = tree_.lower_bound(k);
    if (it == tree_.end() || Compare{}(k, it->first)) {  // ← 直接构造
        it = tree_.insert_unique({k, T{}}).first;
    }
    return const_cast<T&>(it->second);
}
```

---

### 6. hashtable 迭代器跨桶：必须持有哈希表指针

迭代器 `operator++` 在当前链表走完后需要找下一个非空桶，而桶数组 `buckets_` 属于 `hashtable` 对象，迭代器本身若不持有指向哈希表的指针，就无法访问 `buckets_`。

```cpp
template <typename HT>
struct ht_iterator {
    ht_node<typename HT::value_type>* cur_;  // 当前节点
    const HT* ht_;                           // ← 必须持有，用于跨桶查找

    self& operator++() {
        auto old = cur_;
        cur_ = cur_->next;
        if (!cur_) {
            std::size_t idx = ht_->bucket_index(old->value) + 1;
            while (idx < ht_->buckets_.size() && !ht_->buckets_[idx])
                ++idx;
            cur_ = idx < ht_->buckets_.size() ? ht_->buckets_[idx] : nullptr;
        }
        return *this;
    }
};
```

注意迭代器持有的是 `const HT*`，防止通过迭代器意外修改哈希表结构。

---

## 十二、与 std 的对比和差异

### 接口差异总览

| mystl 容器 | std 对应 | 主要差异 |
|-----------|---------|---------|
| `mystl::vector` | `std::vector` | 缺少 `emplace_back`、`shrink_to_fit`、`assign`；无 `noexcept` 保证 |
| `mystl::list` | `std::list` | `sort` 同为归并排序；缺少 `remove_if`、`unique`、双链表 `merge(list&&)` |
| `mystl::deque` | `std::deque` | `BUF` 固定为 512 字节（std 按元素大小动态调整）；缺少 `shrink_to_fit` |
| `mystl::map` / `mystl::set` | `std::map` / `std::set` | 缺少 C++17 `extract`、`merge`；无 hint insert（`insert(iterator, value)`）优化 |
| `mystl::hash_map` | `std::unordered_map` | 命名不同；缺少 `max_load_factor`、`reserve`、`bucket()`；rehash 触发条件为 `size_ >= bucket_count`（而非 `load_factor > max_load_factor`） |
| `mystl::priority_queue` | `std::priority_queue` | 接口基本相同；额外将底层 `sift_up`/`sift_down`/`make_heap` 等函数**直接暴露**在 `mystl` 命名空间 |

### allocator 支持

mystl 容器均**不支持自定义 allocator**：内存分配硬编码使用 `mystl::Allocator<T>`，模板参数 `Alloc` 形同虚设（部分容器甚至未将 `Alloc` 作为模板参数）。`std` 容器的 `allocator_traits` 机制（支持有状态 allocator、`propagate_on_container_swap` 等）在 mystl 中均未实现。

### 异常安全

mystl 容器普遍缺少 `noexcept` 标注和异常安全保证：

```cpp
// std::vector 移动构造保证 noexcept
std::vector<T>(std::vector<T>&&) noexcept;

// mystl::vector 移动构造未标注，强异常安全无法保证
vector(vector&& other);   // 无 noexcept
```

在 `std::vector` 扩容时，若元素的移动构造函数不是 `noexcept`，标准库会退化为拷贝构造以保证强异常安全；mystl 无此机制，一律使用 `std::move`。

### 迭代器失效规则

mystl 与 std 的迭代器失效规则基本一致，但未在代码中强制保障：

| 容器 | 触发失效的操作 |
|------|--------------|
| `vector` | 任何扩容操作（`push_back`/`insert` 触发 `grow`） |
| `deque` | 两端以外的 `insert`/`erase`；map 扩容时所有迭代器 |
| `list` / `hash_map` | 仅被删除元素的迭代器失效，其余不失效 |
| `map` / `set` | 仅被删除元素的迭代器失效，其余不失效 |
