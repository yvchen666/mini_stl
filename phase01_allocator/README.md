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

## 九、运行测试

```bash
# 在项目根目录
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_alloc -j$(nproc)
./build/phase01_allocator/test_alloc
```

预期输出：17 tests, 17 passed。
