# mini_stl — 从零手写 C++ STL 核心组件

> 用约 2000 行 C++11 代码，还原 STL 六大组件的核心实现。
> 每个阶段配有独立的 GTest 测试套件和教程 README。

---

## 快速上手

### 依赖

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| GCC  | ≥ 10    | 系统默认 GCC 7 缺少 C++17 `<filesystem>`，需指定 g++-10 |
| CMake | ≥ 3.14 | 用于 FetchContent 下载 GoogleTest |
| 网络 | 首次构建需要 | FetchContent 自动拉取 GTest v1.14.0 |

### 构建 & 运行

```bash
# 克隆项目
git clone <repo-url>
cd mini_stl

# 配置（指定 GCC 10 以支持 C++17）
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 编译所有阶段
cmake --build build -j$(nproc)

# 运行所有测试
ctest --test-dir build --output-on-failure
```

### 单独运行某个阶段

```bash
# 只编译 + 运行 phase03 的容器测试
cmake --build build --target test_vector test_rb_tree test_map_set -j4
./build/phase03_containers/test_vector
./build/phase03_containers/test_rb_tree
```

### 测试概览

| 阶段 | 可执行文件 | 测试数 |
|------|-----------|--------|
| phase01 | `test_alloc` | 17 |
| phase02 | `test_iterator` | 15 |
| phase03 | `test_vector` `test_list` `test_deque` `test_stack_queue` `test_heap_priority_queue` `test_rb_tree` `test_map_set` `test_hashtable` | 88 |
| phase04 | `test_algorithm` | 25 |
| phase05 | `test_functional` | 21 |
| **合计** | | **166** |

---

## STL 六大组件关系图

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                                                                  用户代码                                                                                                                                                              │
└───────────────────────────┬─────────────────────────────────────┘
                                                                                                  │ 使用
        ┌───────────────────┼───────────────────┐
        ▼                                                                                     ▼                                                                                     ▼
┌──────────────┐  ┌──────────────────┐  ┌─────────────────┐
│   算法                                                  │  │    容器                                                                    │  │   函数对象                                                       │
│  algorithm                                        │  │  vector/list                                                          │  │   plus/less                                                      │
│  sort/find                                          │  │  map/set                                                              │  │  bind2nd/not1                                             │
│  merge/copy                                    │  │  unordered_*                                                    │  │                                                                            │
└──────┬───────┘  └────────┬─────────┘  └────────┬────────┘
                               │ 操作                                                                     │ 存放元素                                                                      │ 传入
                               │                                                                               │                                                                                          │
                               ▼                                                                               ▼                                                                                          │
┌──────────────────────────────────┐                                                │
│           迭代器                                                                                                                                │◄──────────┘
│  iterator_traits / tag dispatch                                                                                            │
│  reverse_iterator / distance                                                                                               │
└──────────────────────────────────┘
       │ 访问内存
       ▼
┌──────────────────────────────────┐
│           内存分配器                                                                                                                      │
│  MallocAlloc (>128B)                                                                                                             │
│  PoolAlloc   (≤128B, freelist)                                                                                             │
│  construct / destroy                                                                                                               │
└──────────────────────────────────┘

  phase01         phase02           phase03        phase04   phase05
  allocator  ──►  iterator  ──►  containers  ──►  algorithm  functor
```

**依赖方向**：左→右，上层依赖下层。每个 phase 只引用前面阶段的头文件。

---

## 各 phase 学习路线导航

| Phase | 主题 | 核心知识点 | 建议前置 |
|-------|------|-----------|---------|
| [phase01](phase01_allocator/README.md) | 内存分配器 | freelist 内存池、OOM 回调、placement new、trivially_destructible 优化 | C++ 内存模型基础 |
| [phase02](phase02_iterator/README.md) | 迭代器与 traits | iterator_traits 偏特化、5 种 tag 继承、tag dispatch、reverse_iterator off-by-one | 模板基础 |
| [phase03](phase03_containers/README.md) | 容器 | vector/list/deque、堆、红黑树（含 insert/erase fixup）、hashtable 开链法 | phase01 + phase02 |
| [phase04](phase04_algorithm/README.md) | 算法 | introsort（快排+堆排+插入排序）、inplace_merge buffer 陷阱、二分查找、排列 | phase02 + phase03 |
| [phase05](phase05_functional/README.md) | 函数对象与适配器 | 仿函数、not1/not2、bind1st/bind2nd、ptr_fun、C++98 vs C++11 对比 | phase03 + phase04 |

---

## STL 知识点总表

> 快速定位实现文件与关键行号。

### 内存管理

| 知识点 | 文件 | 关键位置 |
|--------|------|---------|
| freelist 分配 (`allocate`) | `phase01_allocator/alloc.h` | `PoolAlloc::allocate` |
| freelist 回收 (`deallocate`) | `phase01_allocator/alloc.h` | `PoolAlloc::deallocate` |
| 内存池补充 (`chunk_alloc`) | `phase01_allocator/alloc.h` | `PoolAlloc::chunk_alloc` |
| freelist 借块应急 | `phase01_allocator/alloc.h` | `chunk_alloc` OOM 分支 |
| placement new 封装 | `phase01_allocator/construct.h` | `mystl::construct` |
| trivial 析构优化 | `phase01_allocator/construct.h` | `destroy_aux` tag dispatch |
| memmove 批量拷贝 | `phase01_allocator/uninitialized.h` | `uninitialized_copy_aux` |

### 迭代器

| 知识点 | 文件 | 关键位置 |
|--------|------|---------|
| 5 种 tag 继承 std | `phase02_iterator/iterator_base.h` | tag 定义 |
| iterator_traits 主模板 | `phase02_iterator/iterator_traits.h` | `iterator_traits<T>` |
| T* 偏特化 | `phase02_iterator/iterator_traits.h` | `iterator_traits<T*>` |
| tag dispatch distance | `phase02_iterator/iterator_algo.h` | `distance_aux` 重载 |
| tag dispatch advance | `phase02_iterator/iterator_algo.h` | `advance_aux` 重载 |
| reverse_iterator off-by-one | `phase02_iterator/reverse_iterator.h` | `operator*` |

### 容器

| 知识点 | 文件 | 关键位置 |
|--------|------|---------|
| vector 三指针布局 | `phase03_containers/vector.h` | `start_/finish_/end_of_storage_` |
| vector 容量翻倍 | `phase03_containers/vector.h` | `grow()` / `reallocate()` |
| list 哨兵环 | `phase03_containers/list.h` | `sentinel_` + `begin()`/`end()` |
| list splice O(1) | `phase03_containers/list.h` | `splice()` |
| deque map 指针数组 | `phase03_containers/deque.h` | `map_` + `init_map()` |
| deque 跨 buffer 算术 | `phase03_containers/deque.h` | `iterator::operator+=` |
| sift_up / sift_down | `phase03_containers/heap_priority_queue.h` | `sift_up` / `sift_down` |
| make_heap O(n) | `phase03_containers/heap_priority_queue.h` | `make_heap` |
| rb_tree nil_+header_ 哨兵 | `phase03_containers/rb_tree.h` | 构造函数 |
| rb_tree 插入修复（3 case） | `phase03_containers/rb_tree.h` | `insert_fixup` |
| rb_tree 删除修复（4 case） | `phase03_containers/rb_tree.h` | `erase_fixup` |
| map/set KeyOfValue | `phase03_containers/key_extractors.h` | `Identity` / `SelectFirst` |
| map::operator[] | `phase03_containers/map_set.h` | `map::operator[]` |
| hashtable 素数桶 | `phase03_containers/hashtable.h` | `next_prime` |
| hashtable rehash | `phase03_containers/hashtable.h` | `rehash_if_needed` |

### 算法

| 知识点 | 文件 | 关键位置 |
|--------|------|---------|
| memmove 优化 copy | `phase04_algorithm/algorithm.h` | `copy_aux` + `is_trivially_copy_assignable` |
| introsort 递归深度 | `phase04_algorithm/algorithm.h` | `introsort_depth` |
| median-of-three pivot | `phase04_algorithm/algorithm.h` | `median_of_three` |
| 堆排保底 | `phase04_algorithm/algorithm.h` | `heapsort` + `depth_limit==0` 分支 |
| 插入排序最终通扫 | `phase04_algorithm/algorithm.h` | `sort` 末尾 `insertion_sort` 调用 |
| inplace_merge 缓冲区 | `phase04_algorithm/algorithm.h` | `new T[n1]` + merge |
| lower_bound 循环不变式 | `phase04_algorithm/algorithm.h` | `lower_bound` while 循环 |
| next_permutation 四步法 | `phase04_algorithm/algorithm.h` | `next_permutation` |

### 函数对象与适配器

| 知识点 | 文件 | 关键位置 |
|--------|------|---------|
| unary_function 基类 | `phase05_functional/functional.h` | `unary_function<Arg,Result>` |
| binary_function 基类 | `phase05_functional/functional.h` | `binary_function<A1,A2,R>` |
| unary_negate (not1) | `phase05_functional/functional.h` | `unary_negate` + `not1()` |
| binary_negate (not2) | `phase05_functional/functional.h` | `binary_negate` + `not2()` |
| binder1st / bind1st | `phase05_functional/functional.h` | `binder1st` + `bind1st()` |
| binder2nd / bind2nd | `phase05_functional/functional.h` | `binder2nd` + `bind2nd()` |
| ptr_fun (一元) | `phase05_functional/functional.h` | `pointer_to_unary_function` |
| ptr_fun (二元) | `phase05_functional/functional.h` | `pointer_to_binary_function` |

---

## 项目结构

```
mini_stl/
├── CMakeLists.txt                    # 顶层构建，FetchContent GTest
├── phase01_allocator/
│   ├── alloc.h                       # MallocAlloc + PoolAlloc
│   ├── construct.h                   # construct / destroy
│   ├── uninitialized.h               # uninitialized_copy/fill
│   ├── test_alloc.cpp                # 17 tests
│   └── README.md
├── phase02_iterator/
│   ├── iterator_base.h               # tag 定义 + iterator 基类
│   ├── iterator_traits.h             # iterator_traits 偏特化
│   ├── iterator_algo.h               # distance / advance / next / prev
│   ├── reverse_iterator.h            # reverse_iterator
│   ├── iterator.h                    # 统一 include 入口
│   ├── test_iterator.cpp             # 15 tests
│   └── README.md
├── phase03_containers/
│   ├── vector.h                      # 动态数组
│   ├── list.h                        # 双向循环链表
│   ├── deque.h                       # 双端队列（分段连续内存）
│   ├── stack_queue.h                 # 容器适配器
│   ├── heap_priority_queue.h         # 堆算法 + priority_queue
│   ├── key_extractors.h              # Identity / SelectFirst
│   ├── rb_tree.h                     # 红黑树
│   ├── map_set.h                     # map/set/multimap/multiset
│   ├── hashtable.h                   # hashtable/hash_map/hash_set
│   ├── test_*.cpp                    # 8 个测试文件，88 tests
│   └── README.md
├── phase04_algorithm/
│   ├── algorithm.h                   # 全部算法（单头文件）
│   ├── test_algorithm.cpp            # 25 tests
│   └── README.md
└── phase05_functional/
    ├── functional.h                  # 仿函数 + 适配器（单头文件）
    ├── test_functional.cpp           # 21 tests
    └── README.md
```

---

## 设计原则

- **单头文件**：每个阶段的核心实现集中在一个（或少数几个）`.h` 文件中，便于阅读
- **C++11**：不使用 C++14/17 特性，与 SGI STL 的历史实现风格接近
- **namespace mystl**：与 `std` 隔离，避免命名冲突，同时可与 `std` 容器互操作
- **最小依赖**：每个 phase 只引用前面 phase 的头文件，依赖方向单向

---

## 参考资料

- 侯捷《STL 源码剖析》— 本项目的主要参考，尤其是分配器与容器部分
- cppreference.com — 接口规范参考
- GCC libstdc++ 源码 — introsort、rb_tree 实现参考
