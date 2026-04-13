# Phase 01 — 内存基础：手写分配器

> **目标**：理解 STL 是如何管理内存的，以及为什么小对象分配比裸 `new` 快。

---

## 一、为什么需要自定义分配器？

`operator new` 底层调用 `malloc`，每次调用都需要：
- 系统调用或向 C 运行时申请内存
- 维护堆的元数据（大小、对齐信息等）

对于 STL 容器频繁插入的小对象（如 `list` 的节点，一般 24–32 字节），这个开销不可忽视。**内存碎片**也会随着大量小块申请/释放而累积。

SGI STL 的解法：**双层分配器**。

---

## 二、架构总览

```
用户请求 n 字节
      │
      ├─ n > 128 ──► MallocAlloc（一级）：直接 malloc/free
      │
      └─ n ≤ 128 ──► PoolAlloc（二级）：freelist 内存池
```

---

## 三、一级分配器：MallocAlloc

直接封装 `malloc`/`free`/`realloc`，额外提供 OOM 处理钩子：

```cpp
// 设置 OOM 回调（类似 std::set_new_handler）
MallocAlloc::set_malloc_handler([] {
    std::cerr << "Out of memory! Trying to free cache...\n";
    free_some_cache();  // 你的释放逻辑
});
```

当 `malloc` 返回 `nullptr` 时，分配器会**循环调用**这个回调，直到分配成功或回调未设置时抛出 `std::bad_alloc`。

---

## 四、二级分配器：PoolAlloc（核心）

### 4.1 freelist 结构

16 条链表，每条对应一档内存大小（步长 8 字节）：

```
free_list[0]  →  8字节块 → 8字节块 → nullptr
free_list[1]  → 16字节块 → nullptr
free_list[2]  → 24字节块 → 24字节块 → nullptr
...
free_list[15] → 128字节块 → nullptr
```

每个空闲块的前几个字节被复用为"下一个空闲块的指针"：

```cpp
union FreeNode {
    FreeNode* free_list_link;   // 空闲时：指向下一块
    char      client_data[1];   // 已分配时：这片内存完全归用户
};
```

这个 `union` 技巧是经典 C 内存管理手法：**零额外开销**，空闲块的链表指针和用户数据共用同一片内存。

### 4.2 分配流程

```
allocate(n)
  │
  ├─ n > 128 ──► MallocAlloc::allocate(n)
  │
  └─ n ≤ 128 ──► 对齐到 8 的倍数，查找对应 freelist
        │
        ├─ freelist 有空块 ──► 摘出头节点，返回
        │
        └─ freelist 为空 ──► refill()
              │
              └─ chunk_alloc() 从内存池切出 20 个块
                    │
                    ├─ 内存池够 ──► 切出，第一个返回，其余挂链
                    │
                    └─ 内存池不够 ──► malloc 申请 2×requested 的大块
                          │
                          └─ malloc 失败 ──► 在更大档的 freelist 里借一块
```

### 4.3 释放流程

释放时**不归还给 OS**，直接头插回对应 freelist：

```cpp
deallocate(p, 8)
  └─ free_list[0] ← p ← (原头节点)
```

这意味着内存池的内存只在进程结束时才释放。这是一个**有意的设计取舍**：
- 优点：极快，零系统调用
- 缺点：长期运行的服务进程内存不会缩减（但 STL 容器本来也不会主动归还）

---

## 五、关键代码片段

### 对齐计算

```cpp
// 将 n 上取整到 ALIGN(8) 的倍数
// 例：n=13 → (13+7)&~7 = 16
static std::size_t round_up(std::size_t n) {
    return (n + ALIGN - 1) & ~(ALIGN - 1);
}

// 确定落在哪条 freelist（0-based）
// 例：n=8  → (8+7)/8 - 1 = 0
//     n=16 → (16+7)/8 - 1 = 1
//     n=128→ (128+7)/8 - 1 = 15
static std::size_t freelist_index(std::size_t n) {
    return (n + ALIGN - 1) / ALIGN - 1;
}
```

### 内存池耗尽时的应急策略

```cpp
// malloc 失败后，从更大档位的 freelist 里找一块救急
for (std::size_t s = size + ALIGN; s <= MAX_BYTES; s += ALIGN) {
    std::size_t idx = freelist_index(s);
    if (free_list_[idx]) {
        pool_start_ = reinterpret_cast<char*>(free_list_[idx]);
        free_list_[idx] = free_list_[idx]->free_list_link;
        pool_end_ = pool_start_ + s;
        return chunk_alloc(size, n_obj);  // 递归重试
    }
}
```

---

## 六、construct.h — 对象生命周期原语

```cpp
// placement new：在已有内存 p 上构造对象，不分配新内存
mystl::construct(p, args...);

// 显式调用析构函数：只析构，不释放内存
mystl::destroy(p);

// 批量析构：对 trivially_destructible 类型走空路径（零开销）
mystl::destroy(first, last);
```

`destroy(first, last)` 内部通过 `std::is_trivially_destructible<T>` 做编译期分发：
- `int`、`double` 等基本类型：什么都不做（析构函数为空）
- `std::string`、自定义类型：逐个调用 `~T()`

---

## 七、uninitialized.h — 批量构造原语

| 函数 | 作用 |
|------|------|
| `uninitialized_copy(first, last, result)` | 拷贝构造到未初始化内存 |
| `uninitialized_fill(first, last, value)` | 用 value 填充构造 |
| `uninitialized_fill_n(first, n, value)` | 构造 n 个对象 |

对 `trivially_copy_assignable` 类型（如 `int`），自动调用 `memmove`，避免逐个构造的开销。

---

## 八、常见问题

**Q: freelist 里的内存是哪来的？**

来自 `chunk_alloc()`，后者向 `malloc` 申请大块内存（2 × 请求量 + 历史申请量/16），切分后分发。首次请求 8 字节时，`chunk_alloc` 可能一次性切出 20 个 8 字节块，19 个挂到 freelist，1 个立即返回。

**Q: 为什么释放后不还给 OS？**

还给 OS 需要 `free()`，而 freelist 里的块可能来自一大块连续内存的中间，无法单独 `free`。SGI 选择不归还，接受这个内存"只增不减"的行为，换取极高的分配速度。

**Q: 多线程安全吗？**

原始 SGI STL 的二级分配器**不是线程安全的**。生产环境需要加锁或使用线程本地存储（TLS）。本实现忠于原版，同样不做同步。

---

## 九、复杂度分析表

| 操作 | 时间复杂度 | 空间复杂度 | 备注 |
|------|-----------|-----------|------|
| `allocate(n≤128)` | O(1) | O(1) | freelist 头节点摘出，无系统调用 |
| `allocate(n>128)` | O(1) 均摊 | O(n) | 直接 `malloc`，受 OS 调度影响 |
| `deallocate(n≤128)` | O(1) | O(1) | 头插回 freelist，无系统调用 |
| `chunk_alloc` (cold) | O(1) 均摊 | O(n) | 首次调用 `malloc` 申请大块，切分挂链 |
| `refill` | O(1) | O(n) | 调用 `chunk_alloc` 后将多余块挂入 freelist |
| `construct(T, args)` | O(1) | O(1) | placement new，不分配内存 |
| `destroy(T*)` | O(1) | O(1) | 直接调用 `~T()`，不释放内存 |
| `uninitialized_copy` (trivial type) | O(n) | O(1) | 退化为 `memmove`，常数极小 |
| `uninitialized_copy` (non-trivial) | O(n) | O(1) | 逐元素 placement new，可能抛异常 |

---

## 十、代码走读（逐步图解）

### allocate(13) 完整路径

```
调用 allocate(13)
 1. n=13 ≤ 128，进入二级分配器路径
 2. round_up(13) = (13+7)&~7 = 16  → 落在 free_list[1]
 3. free_list[1] 当前有空块：
    free_list[1] → [16B块A] → [16B块B] → nullptr
 4. 摘出头节点 A：
    p = free_list[1]               // p 指向块A
    free_list[1] = p->free_list_link  // 链表前进到块B
    free_list[1] → [16B块B] → nullptr
 5. 返回块A地址（用户可用 16 字节）
```

### deallocate(p, 13) 归还路径

```
调用 deallocate(p, 13)
 1. n=13 ≤ 128，进入二级分配器路径
 2. round_up(13) = 16 → 落在 free_list[1]
 3. 将 p 头插回 free_list[1]：
    reinterpret_cast<FreeNode*>(p)->free_list_link = free_list[1]
    free_list[1] = reinterpret_cast<FreeNode*>(p)
    free_list[1] → [归还的块A] → [16B块B] → nullptr
 4. 内存不还给 OS，等待下次 allocate 直接复用
```

### chunk_alloc 当 freelist 为空时

```
初始状态：free_list[1] = nullptr，pool_start_ = pool_end_（内存池耗尽）

chunk_alloc(size=16, n_obj=20) 被 refill 调用
 1. 计算需要字节：total = 16 * 20 = 320B
 2. pool_start_ == pool_end_，内存池为空
 3. 调用 malloc(2 * 320 + heap_size_/16) = malloc(640 + 附加量)
    pool_start_ ──────────────────────────────────────────┐
    pool_end_   ────────────────────────────────────── (640B后)
                [  可用内存块 640B                        ]
 4. 从 pool_start_ 切出 n_obj=20 个 16B 块：
    [块0][块1][块2]...[块19]  pool_start_ 前移 320B
 5. 块0 直接返回给调用者，块1~块19 由 refill 挂入 free_list[1]
    free_list[1] → [块1] → [块2] → ... → [块19] → nullptr
```

---

## 十一、实现难点 & 踩坑记录

### 1. freelist_index 计算陷阱

`n / ALIGN - 1` 在 `n=0` 时会发生无符号下溢（`std::size_t` 为无符号类型）：

```cpp
// 错误写法：n=0 时 0/8=0，0-1 = SIZE_MAX，越界访问 free_list_
static std::size_t freelist_index_wrong(std::size_t n) {
    return n / ALIGN - 1;
}

// 正确写法：先加 ALIGN-1 再除，n=0 → (0+7)/8-1 = 0 （仍需上游保证 n>0）
static std::size_t freelist_index(std::size_t n) {
    return (n + ALIGN - 1) / ALIGN - 1;
}
```

实际调用点要保证 `n > 0`，或在 `allocate` 入口处对 `n==0` 特殊处理（返回 `allocate(1)`）。

### 2. chunk_alloc 递归终止条件

当内存池耗尽且 `malloc` 失败后，代码会遍历更大档位的 freelist 借一块救急，再**递归**调用 `chunk_alloc`。若所有 freelist 都为空，必须终止递归并抛出异常：

```cpp
// 正确：所有救急路径都走完后抛出
throw std::bad_alloc();

// 错误：忘记终止条件，导致无限递归直到栈溢出
// return chunk_alloc(size, n_obj);  // ← 没有任何状态改变时不能递归
```

### 3. static 成员的 ODR 问题

`PoolAlloc` 的 freelist 是 `static` 数据成员，在多个翻译单元中使用时必须恰好定义一次：

```cpp
// pool_alloc.h（声明）
class PoolAlloc {
    static FreeNode* free_list_[N_FREELISTS];
};

// pool_alloc.cpp（定义，C++14 及以前必须有这行）
PoolAlloc::FreeNode* PoolAlloc::free_list_[PoolAlloc::N_FREELISTS] = {};

// C++17 替代方案：头文件中用 inline static
class PoolAlloc {
    inline static FreeNode* free_list_[N_FREELISTS] = {};  // 无需 .cpp 定义
};
```

忘记在 `.cpp` 中定义会在链接阶段报 `undefined reference`，而不是编译错误，容易排查困难。

---

## 十二、与 std 的对比和差异

### mystl::Allocator 与 std::allocator 接口对比

| 接口 | mystl::Allocator | std::allocator | 备注 |
|------|-----------------|----------------|------|
| `allocate(n)` | 有，≤128 走内存池 | 有，直接 `operator new` | mystl 有分配加速 |
| `deallocate(p, n)` | 有，≤128 归还 freelist | 有，`operator delete` | mystl 不还给 OS |
| `construct(p, args)` | 有，placement new | 有（C++17 前是成员，后移至 `std::allocator_traits`） | 语义相同 |
| `destroy(p)` | 有，显式析构 | 有（同上） | 语义相同 |

### 主要差异

- **std::allocator**：现代标准库实现（libc++、libstdc++）通常直接调用 `operator new`/`operator delete`，不使用内存池，依赖 tcmalloc / jemalloc 等系统分配器的优化。
- **mystl::PoolAlloc**：实现的是 SGI STL（1990 年代）的二级分配器，属于历史产物，目的是在没有高性能系统分配器时提速小对象分配。

### mystl 缺失的 C++11 Allocator 要求

```cpp
// 以下 traits 在 mystl 中未实现：
allocator_traits<A>::rebind_alloc<U>          // 容器内部节点类型转换
allocator_traits<A>::max_size(a)              // 最大可分配大小
allocator_traits<A>::propagate_on_container_copy_assignment  // 拷贝时是否传播
allocator_traits<A>::propagate_on_container_move_assignment  // 移动时是否传播
allocator_traits<A>::is_always_equal          // 两个实例是否等价
```

缺少 `rebind` 会导致 `std::list`、`std::map` 等节点容器无法使用 mystl 分配器（它们需要 `rebind` 为节点类型分配内存）。

---

## 十三、运行测试

```bash
# 在项目根目录
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_alloc -j$(nproc)
./build/phase01_allocator/test_alloc
```

预期输出：17 tests, 17 passed。
