# Phase 04 — 算法库（algorithm.h）

> mini_stl 系列教程 · 第四阶段
>
> 本阶段在 `namespace mystl` 下实现了 C++ 标准库 `<algorithm>` 中最核心的一批算法，
> 重点是 **introsort**（`std::sort` 的真实算法）和 **归并**（含原地归并的 buffer 陷阱）。

---

## 目录

1. [整体概览](#整体概览)
2. [基础工具 — min / max / swap / iter_swap](#基础工具)
3. [查找 — find / find_if](#查找)
4. [拷贝 — copy / copy_backward](#拷贝)
5. [填充 — fill / fill_n](#填充)
6. [反转 — reverse](#反转)
7. [排序 — introsort 详解](#排序--introsort-详解)
   - [为什么不直接用快排？](#为什么不直接用快排)
   - [三路组合策略](#三路组合策略)
   - [Pivot 选取：三数中值](#pivot-选取三数中值)
   - [递归深度限制与堆排序保底](#递归深度限制与堆排序保底)
   - [插入排序收尾](#插入排序收尾)
   - [完整调用链](#完整调用链)
8. [归并 — merge / inplace_merge](#归并)
   - [merge：两路归并到输出迭代器](#merge两路归并到输出迭代器)
   - [inplace_merge：原地归并的陷阱与正确做法](#inplace_merge原地归并的陷阱与正确做法)
9. [二分查找 — lower_bound / upper_bound / binary_search](#二分查找)
10. [排列 — next_permutation / prev_permutation](#排列)
11. [统计 — count / count_if / equal](#统计)
12. [测试覆盖与运行方式](#测试覆盖与运行方式)
13. [小结](#小结)

---

## 整体概览

```
algorithm.h
├── 基础工具       min, max, swap, iter_swap
├── 查找           find, find_if
├── 拷贝           copy (memmove 优化), copy_backward
├── 填充           fill (memset 优化), fill_n
├── 反转           reverse
├── 排序           sort → introsort (快排 + 堆排 + 插入排序)
├── 归并           merge, inplace_merge
├── 二分查找       lower_bound, upper_bound, binary_search
├── 排列           next_permutation, prev_permutation
└── 统计           count, count_if, equal
```

所有函数都在 `namespace mystl` 内，依赖前两个阶段的成果：

- `phase02_iterator/iterator.h` — 提供 `distance`、`next`、迭代器 tag
- `phase03_containers/heap_priority_queue.h` — 提供 `make_heap`、`sort_heap`（introsort 保底用）

---

## 基础工具

### min / max

```cpp
template <typename T>
const T& min(const T& a, const T& b) { return b < a ? b : a; }

template <typename T>
const T& max(const T& a, const T& b) { return a < b ? b : a; }
```

只依赖 `operator<`，返回引用，避免拷贝。

### swap / iter_swap

```cpp
template <typename T>
void swap(T& a, T& b) noexcept {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}

template <typename ForwardIt>
void iter_swap(ForwardIt a, ForwardIt b) {
    mystl::swap(*a, *b);
}
```

`swap` 用移动语义，对容器类型（vector、string 等）开销是 O(1)。
`iter_swap` 对迭代器解引用后再 swap，是排序算法内部的标准原语。

---

## 查找

### find

线性扫描，遇到相等元素立刻返回：

```cpp
template <typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value) {
    while (first != last && !(*first == value)) ++first;
    return first;
}
```

未找到时返回 `last`，调用方用 `it != last` 判断结果是否有效。

### find_if

接受谓词（predicate），可表达任意条件：

```cpp
template <typename InputIt, typename Pred>
InputIt find_if(InputIt first, InputIt last, Pred pred) {
    while (first != last && !pred(*first)) ++first;
    return first;
}
```

**示例：** 找第一个偶数

```cpp
int arr[] = {1, 3, 5, 6, 7};
auto it = mystl::find_if(arr, arr + 5, [](int x){ return x % 2 == 0; });
// *it == 6
```

---

## 拷贝

### copy — 针对平凡类型的 memmove 优化

C++ 标准把类型分为"平凡可拷贝赋值"（trivially copy assignable）和"非平凡"两类。
对 `int`、`double`、指针等 POD 类型，直接调 `memmove` 比逐元素赋值快得多。
`copy_aux` 通过标签分发（tag dispatch）在编译期选择路径：

```cpp
// 平凡类型：memmove 加速
template <typename T>
T* copy_aux(const T* first, const T* last, T* result, std::true_type) {
    std::size_t n = static_cast<std::size_t>(last - first);
    std::memmove(result, first, n * sizeof(T));
    return result + n;
}

// 非平凡类型：逐元素赋值（确保构造/析构语义正确）
template <typename InputIt, typename OutputIt>
OutputIt copy_aux(InputIt first, InputIt last, OutputIt result, std::false_type) {
    while (first != last) *result++ = *first++;
    return result;
}

template <typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt result) {
    using T = typename std::iterator_traits<InputIt>::value_type;
    return copy_aux(first, last, result,
        std::is_trivially_copy_assignable<T>{});
}
```

注意使用 `memmove` 而非 `memcpy`：`memmove` 允许源区和目标区重叠，更安全。

### copy_backward

从尾部向前拷贝，用于区间向右移位时不能用 `copy`（否则会覆盖未读数据）：

```cpp
template <typename BidirIt1, typename BidirIt2>
BidirIt2 copy_backward(BidirIt1 first, BidirIt1 last, BidirIt2 result) {
    while (last != first) *--result = *--last;
    return result;
}
```

---

## 填充

### fill — char 类型用 memset 优化

```cpp
// 通用版本
template <typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value) {
    while (first != last) *first++ = value;
}

// char* 特化：memset 是 C 库中最快的填充原语
inline void fill(char* first, char* last, char value) {
    std::memset(first, static_cast<unsigned char>(value),
                static_cast<std::size_t>(last - first));
}
```

### fill_n

```cpp
template <typename OutputIt, typename Size, typename T>
OutputIt fill_n(OutputIt first, Size n, const T& value) {
    while (n-- > 0) *first++ = value;
    return first;
}
```

`fill_n` 返回指向最后写入位置之后的迭代器，方便链式调用。

---

## 反转

```cpp
template <typename BidirIt>
void reverse(BidirIt first, BidirIt last) {
    while (first != last && first != --last)
        iter_swap(first++, last);
}
```

双指针从两端向中间收拢，每次交换头尾元素。
条件 `first != --last` 确保奇数长度时中间元素不被自我交换。

---

## 排序 — introsort 详解

`mystl::sort` 是本阶段最核心的算法，采用与 libstdc++ 相同的 **introsort** 策略。

### 为什么不直接用快排？

朴素快排有两个已知缺陷：

| 问题 | 具体场景 | 后果 |
|------|---------|------|
| 最坏 O(n²) | 已排序数组 + 固定 pivot（如取首元素）| 退化成 n 层递归，栈溢出 |
| 常数因子大 | 小数组（n < 16）| 函数调用和分区开销远超插入排序 |

introsort 用三种算法组合，取各自优点，规避缺点。

### 三路组合策略

```
introsort = 快速排序（主体）
          + 堆排序（深度超限时的保底）
          + 插入排序（小分区 + 最终收尾）
```

```
sort(first, last)
│
├── 计算深度上限 depth = introsort_depth(n)
│
└── introsort(first, last, depth)
    │
    ├── [区间长度 <= 16] ──── 不处理，留给最终插入排序
    │
    ├── [depth_limit == 0] ─ heapsort(first, last)  ← 保底，O(n log n) 最坏
    │                         return
    │
    └── [否则] 快速排序分区
              ├── pivot = median_of_three(first, mid, last-1)
              ├── 分区后递归处理右半（较小分区）
              └── 循环处理左半（尾递归优化）

最后：insertion_sort(first, last)  ← 对近乎有序的全局数组做一遍
```

### Pivot 选取：三数中值

朴素快排取首元素做 pivot，对已排序数组退化到 O(n²)。
**三数中值**（median-of-three）取首、中、尾三个位置的中间值，
大幅降低退化概率：

```cpp
template <typename RandomIt, typename Compare>
RandomIt median_of_three(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
    if (comp(*a, *b)) {
        if (comp(*b, *c)) return b;   // a < b < c，中值是 b
        if (comp(*a, *c)) return c;   // a < c <= b，中值是 c
        return a;                      // c <= a < b，中值是 a
    }
    // b <= a
    if (comp(*a, *c)) return a;        // b <= a < c，中值是 a
    if (comp(*b, *c)) return c;        // b < c <= a，中值是 c
    return b;                           // c <= b <= a，中值是 b
}
```

选出 pivot 后 `iter_swap(pivot, last-1)`，把它放到末尾，方便 Hoare 分区。

### 递归深度限制与堆排序保底

深度上限的计算公式：

```cpp
inline std::ptrdiff_t introsort_depth(std::ptrdiff_t n) {
    std::ptrdiff_t depth = 0;
    while (n > 1) { n >>= 1; depth += 2; }
    return depth;
}
```

本质是 `2 * floor(log2(n))`：每次 `n >>= 1` 相当于对 n 取 log₂，乘以 2 给出两倍余量，
即允许快排在退化路径上多跑一倍深度，才切换到堆排序。

当 `depth_limit` 递减到 0 时，说明分区已经严重不均衡（接近退化情况），
立刻切换到堆排序，保证剩余区间以 O(n log n) 最坏复杂度结束：

```cpp
template <typename RandomIt, typename Compare>
void heapsort(RandomIt first, RandomIt last, Compare comp) {
    mystl::make_heap(first, last, comp);   // O(n) 建堆
    mystl::sort_heap(first, last, comp);   // O(n log n) 堆排序
}
```

### introsort 主体

```cpp
template <typename RandomIt, typename Compare>
void introsort(RandomIt first, RandomIt last, std::ptrdiff_t depth_limit,
               Compare comp) {
    while (last - first > INSERTION_SORT_THRESHOLD) {  // 阈值 16
        if (depth_limit == 0) {
            heapsort(first, last, comp);   // 深度超限 → 堆排兜底
            return;
        }
        --depth_limit;

        // 三数中值选 pivot，移到末尾
        auto mid = first + (last - first) / 2;
        auto pivot = median_of_three(first, mid, last - 1, comp);
        iter_swap(pivot, last - 1);

        // Hoare 风格分区
        auto p = first, q = last - 2;
        auto pivot_val = *(last - 1);
        while (true) {
            while (comp(*p, pivot_val)) ++p;            // 找左侧 >= pivot 的
            while (p < q && !comp(*q, pivot_val)) --q; // 找右侧 < pivot 的
            if (p >= q) break;
            iter_swap(p, q);
        }
        iter_swap(p, last - 1);   // pivot 归位

        // 只递归右半（较小分区），循环处理左半（尾递归优化，减少栈深度）
        introsort(p + 1, last, depth_limit, comp);
        last = p;
    }
}
```

**尾递归优化（tail-call optimization）：** 快排最坏情况下栈深度为 O(n)，
总是先递归**较小**分区（右半 `[p+1, last)`），
然后用 `last = p` 循环处理**较大**分区（左半 `[first, p)`），
确保递归栈深度为 O(log n)。

### 插入排序收尾

```cpp
template <typename RandomIt, typename Compare>
void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
    if (first == last) return;
    for (auto i = first + 1; i != last; ++i) {
        auto value = std::move(*i);
        auto j = i;
        while (j != first && comp(value, *(j - 1))) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(value);
    }
}
```

introsort 结束后，整个数组已被分成若干"几乎有序"的小段（每段长度 ≤ 16）。
此时对**整个数组**跑一遍插入排序，可以用 O(n) 的扫描完成收尾
（每个元素只需移动很短的距离），常数因子远比再次递归调用快排小。

### 完整调用链

```cpp
template <typename RandomIt, typename Compare>
void sort(RandomIt first, RandomIt last, Compare comp) {
    if (last - first < 2) return;
    std::ptrdiff_t depth = introsort_depth(last - first);
    introsort(first, last, depth, comp);   // 把大分区排成"近乎有序"
    insertion_sort(first, last, comp);     // 最终通扫，处理所有分区尾部
}

// 无比较器版本，默认 std::less
template <typename RandomIt>
void sort(RandomIt first, RandomIt last) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    mystl::sort(first, last, std::less<T>{});
}
```

**复杂度总结：**

| 情形 | 复杂度 |
|------|-------|
| 平均 | O(n log n) |
| 最坏（递归退化，堆排保底）| O(n log n) |
| 已排序小数组 | O(n)（插入排序线性扫描） |
| 空间（栈）| O(log n) |

---

## 归并

### merge：两路归并到输出迭代器

经典两路归并，要求两个输入范围各自有序，结果写入 `result`：

```cpp
template <typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt merge(InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2,
               OutputIt result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first2, *first1)) *result++ = *first2++;
        else                        *result++ = *first1++;
    }
    // 将剩余元素全部追加（两个 copy 嵌套调用）
    return mystl::copy(first2, last2,
           mystl::copy(first1, last1, result));
}
```

比较逻辑：`comp(*first2, *first1)` 而非 `comp(*first1, *first2)`——
当 `*first2 < *first1` 时取右侧，否则取左侧。这保证了归并的**稳定性**：
相等元素时优先取左侧，左侧元素的相对顺序在输出中保持不变。

### inplace_merge：原地归并的陷阱与正确做法

`inplace_merge(first, middle, last)` 要将 `[first, middle)` 和 `[middle, last)`
两段已排序的区间**就地**合并成一个有序区间，输出仍写回 `[first, last)`。

#### 错误做法：直接原地归并（double-pickup 问题）

初看下面这种写法似乎可行——用两个指针 `l`、`r` 和一个输出指针 `out` 在同一数组上操作：

```
[first ............. middle ............... last)
  ^-- l (左半)         ^-- r (右半)
  ^-- out (输出从头覆盖)
```

问题出在 `out` 追上 `r` 之前，`out` 会**覆盖**左半还未读取的位置。
更危险的情况发生在这样一个时序里：

```
初始: [1, 3, 5 | 2, 4, 6]
       l=0       r=3     out=0

step1: *r(2) < *l(3)，写入 arr[0]=2，out=1，r=4
       此时 arr 变成 [2, 3, 5 | 2, 4, 6]
                     ↑ 刚写的 2

step2: *l(3) <= *r(4)，写入 arr[1]=3，out=2，l=1
       arr 变成 [2, 3, 5 | 2, 4, 6]  (l 现在指向 arr[1] = 3，刚写入的值)
```

当 `out` 指针向右推进时，它会把**右半的元素 2** 写到 `arr[0]`，
而这个位置原本是 **左半的元素 1**——此时元素 1 就**永久丢失**了。
`r` 指针后来还会再次路过 `arr[0]` 附近（如果 `r` 先越过 `out`），
捡起已经被覆盖的假数据，即"double-pickup（重复拾取）"。

#### 正确做法：先把左半拷贝到缓冲区

```cpp
template <typename BidirIt, typename Compare>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last, Compare comp) {
    using T = typename std::iterator_traits<BidirIt>::value_type;

    // 1. 把左半段 [first, middle) 移到独立缓冲区，彻底隔离读写区域
    std::size_t n1 = static_cast<std::size_t>(mystl::distance(first, middle));
    T* buf = new T[n1];
    for (std::size_t i = 0; i < n1; ++i) buf[i] = std::move(first[i]);

    // 2. 从 buf（原左半）和 [middle, last)（右半，仍在原数组）归并回 [first, last)
    T* lb = buf, *le = buf + n1;  // 左半游标（在 buf 中）
    BidirIt ri = middle;           // 右半游标（在原数组中）
    BidirIt out = first;           // 输出游标（从 first 开始覆盖原数组）

    while (lb != le && ri != last) {
        if (comp(*ri, *lb)) *out++ = std::move(*ri++);
        else                *out++ = std::move(*lb++);
    }
    while (lb != le)   *out++ = std::move(*lb++);
    while (ri != last) *out++ = std::move(*ri++);

    delete[] buf;
}
```

关键思路：将左半移到 `buf` 后，`buf` 和原数组的 `[middle, last)` 是两块**不重叠**的独立区域，
`out` 向右推进时只会覆盖已经读过的旧数据，不存在"先写后读同一位置"的问题。

**示意图（修正后）：**

```
buf:        [1, 3, 5]        ← 左半的独立副本
原数组:     [?, ?, ? | 2, 4, 6]
             out↑       ri↑

归并时 out 写入原数组，始终追在 ri 后面或写入已复制走的 [first, middle) 区域。
不论如何，buf 和 [middle, last) 都不会被 out 破坏。
```

---

## 二分查找

三个算法均基于**半开区间 `[first, last)`**，通过 `mystl::distance` 和 `mystl::next` 支持非随机访问迭代器（ForwardIterator 即可）。

### lower_bound — 第一个 >= value 的位置

```cpp
template <typename ForwardIt, typename T, typename Compare>
ForwardIt lower_bound(ForwardIt first, ForwardIt last,
                      const T& value, Compare comp) {
    auto n = mystl::distance(first, last);
    while (n > 0) {
        auto half = n / 2;
        auto mid = mystl::next(first, half);
        if (comp(*mid, value)) {
            first = mystl::next(mid);  // mid < value，往右缩
            n -= half + 1;
        } else {
            n = half;                  // mid >= value，往左缩
        }
    }
    return first;
}
```

循环不变式：答案始终在 `[first, first + n)` 内。
退出时 `n == 0`，`first` 即为结果。

### upper_bound — 第一个 > value 的位置

与 `lower_bound` 的唯一区别在判断条件取反——

```cpp
if (!comp(value, *mid))   // mid <= value，往右缩
```

而 `lower_bound` 是 `comp(*mid, value)`（mid < value 时往右）。
两者合用可以表示一个重复元素的等值区间 `[lower, upper)`。

### binary_search — 存在性查询

```cpp
template <typename ForwardIt, typename T, typename Compare>
bool binary_search(ForwardIt first, ForwardIt last,
                   const T& value, Compare comp) {
    auto it = mystl::lower_bound(first, last, value, comp);
    return it != last && !comp(value, *it);
}
```

找到 `lower_bound` 后还需检查：
- `it != last`：`lower_bound` 没有到达末尾（说明可能存在 >= value 的元素）
- `!comp(value, *it)`：即 `*it <= value`，结合 `*it >= value`（lower_bound 的语义），得 `*it == value`

---

## 排列

### next_permutation — 生成下一个字典序排列

算法步骤：

1. 从右向左找到第一个"下降点" `i`，使 `*i < *(i+1)`（右侧已是降序）
2. 从右向左找到第一个比 `*i` 大的元素 `j`
3. 交换 `*i` 和 `*j`
4. 反转 `[i+1, last)`（将降序变升序，得到下一个最小排列）

```cpp
template <typename BidirIt, typename Compare>
bool next_permutation(BidirIt first, BidirIt last, Compare comp) {
    if (first == last) return false;
    auto i = last;
    if (first == --i) return false;
    for (;;) {
        auto ip1 = i;
        if (comp(*--i, *ip1)) {            // 找到下降点 i
            auto j = last;
            while (!comp(*i, *--j));       // 找右侧第一个比 *i 大的 j
            iter_swap(i, j);
            mystl::reverse(ip1, last);
            return true;
        }
        if (i == first) {
            mystl::reverse(first, last);   // 已是最大排列，绕回最小
            return false;
        }
    }
}
```

**示例：** `{1,2,3}` 的全部 6 个排列

```cpp
int arr[] = {1, 2, 3};
do {
    // 使用 arr...
} while (mystl::next_permutation(arr, arr + 3));
// 循环结束后 arr 恢复为 {1, 2, 3}
```

### prev_permutation — 生成上一个字典序排列

逻辑与 `next_permutation` 对称，找"上升点"（`*i > *(i+1)`），从右找第一个比 `*i` 小的交换，再反转后缀。

---

## 统计

### count / count_if

```cpp
// 计数等于 value 的元素个数
template <typename InputIt, typename T>
std::ptrdiff_t count(InputIt first, InputIt last, const T& value) {
    std::ptrdiff_t n = 0;
    while (first != last) { if (*first++ == value) ++n; }
    return n;
}

// 计数满足谓词的元素个数
template <typename InputIt, typename Pred>
std::ptrdiff_t count_if(InputIt first, InputIt last, Pred pred) {
    std::ptrdiff_t n = 0;
    while (first != last) { if (pred(*first++)) ++n; }
    return n;
}
```

### equal

```cpp
template <typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
    while (first1 != last1) if (!(*first1++ == *first2++)) return false;
    return true;
}
```

只接受第一个范围的 `last`，第二个范围假定长度足够（调用方保证）。

---

## 测试覆盖与运行方式

测试文件为 `test_algorithm.cpp`，使用 Google Test 框架，覆盖以下场景：

| 分类 | 测试用例 |
|------|---------|
| find / find_if | 基本查找、未找到、谓词查找 |
| copy / fill | trivial 类型、non-trivial（string）、fill_n 边界 |
| reverse | 奇偶长度数组 |
| sort | 随机、已排序、逆序、含重复、降序比较器、1000 元素大数组 |
| merge | 两路归并正确性 |
| inplace_merge | 原地归并正确性 |
| lower/upper_bound | 精确匹配、区间查找 |
| binary_search | 存在/不存在 |
| next/prev_permutation | 全排列计数（=6）、具体排列值、绕回行为 |
| count / count_if / equal | 计数、谓词计数、相等/不相等 |

构建与运行（在 `phase04_algorithm/` 目录下）：

```bash
mkdir -p build && cd build
cmake .. && make
./test_algorithm
```

---

## 复杂度分析表

| 算法 | 时间复杂度 | 空间复杂度 | 稳定性 | 备注 |
|------|-----------|----------|--------|------|
| find/find_if | O(n) | O(1) | 稳定 | 线性扫描 |
| copy (trivial) | O(n) | O(1) | 稳定 | memmove 实现 |
| copy (non-trivial) | O(n) | O(1) | 稳定 | 逐元素赋值 |
| fill | O(n) | O(1) | - | char 特化用 memset |
| reverse | O(n) | O(1) | 不稳定 | 双指针对调 |
| sort (average) | O(n log n) | O(log n) | 不稳定 | introsort |
| sort (worst) | O(n log n) | O(log n) | 不稳定 | 堆排保底 |
| merge | O(n+m) | O(1) | 稳定 | 需要输出迭代器 |
| inplace_merge | O(n) | O(n/2) | 稳定 | 需要左半缓冲区 |
| lower_bound/upper_bound | O(log n) | O(1) | - | 要求有序 |
| binary_search | O(log n) | O(1) | - | 内部调 lower_bound |
| next_permutation | O(n) | O(1) | - | 四步法 |
| count/count_if | O(n) | O(1) | - | 线性扫描 |

---

## 代码走读：introsort 分区过程图解

以小数组 `[5, 3, 8, 1, 7]`（size=5）为例逐步追踪：

```
初始: [5, 3, 8, 1, 7]
depth_limit = 2*floor(log2(5)) = 4

Step 1: size=5 > THRESHOLD(16)? 否，直接插入排序
  → 等等，size=5 < 16，introsort 退出，insertion_sort 做最终通扫

再换更大例子：[5, 3, 8, 1, 7, 2, 4, 6]  (size=8)
Step 1: size=8 < 16 → introsort 直接返回，insertion_sort 通扫
```

换用 20 个元素的例子，展示 pivot 选取过程：

```
median_of_three(first=arr[0], mid=arr[10], last-1=arr[19], comp)
  *arr[0]=15, *arr[10]=7, *arr[19]=3
  comp(15,7)=false, comp(15,3)=false, comp(7,3)=false → return arr[19] (3)
  → pivot=3
```

选出 pivot=3 后执行 `iter_swap(pivot, last-1)`，将 3 放到末尾，然后进入分区循环：

```
分区前: [..., 3]   pivot 在 last-1 位置
  p = first, q = last-2

  内层循环：
    p 向右扫，直到 *p >= pivot(3)
    q 向左扫，直到 *q < pivot(3)
    若 p < q，swap(*p, *q)，继续
    若 p >= q，退出

  最后：iter_swap(p, last-1)  → pivot 归位到 p 处
  左半 [first, p) 全 < pivot，右半 (p, last) 全 >= pivot
```

递归只对右半（较小分区）调用 introsort，左半用尾递归循环处理，栈深度始终为 O(log n)。

---

## 实现难点 & 踩坑记录

1. **inplace_merge 双重读取 bug**：原地归并时，left 指针 `f` 和 right 指针 `m` 共享内存。当我们把 `*m` 写入 `out` 位置后，`out` 推进覆盖了原来 `f` 指向的数据。具体场景：
   ```
   arr: [1, 3, 5, | 2, 4, 6]  分界在 arr+3
   out=arr+0, f=arr+0, m=arr+3
   比较 *f(1) vs *m(2) → 写 *f(1) 到 *out(arr[0])  — 原地没动
   out=arr+1, f=arr+1
   比较 *f(3) vs *m(2) → 写 *m(2) 到 *out(arr[1])  — arr[1] 从 3 变成 2
   f=arr+1 现在读到的是已被覆盖的 2，不是原来的 3 — 数据错误！
   ```
   修复：先把左半段复制到 `new T[n1]` 缓冲区。

2. **ADL 歧义 — inplace_merge 无参版调用有参版**：`inplace_merge(int*, int*, int*)` 内部调 `inplace_merge(int*, int*, int*, less<int>)` 时，ADL 同时找到 `mystl::` 和 `std::` 的版本。用 `::mystl::inplace_merge(...)` 显式加前缀解决。

3. **sort 的插入排序"最终通扫"位置**：`insertion_sort` 必须在 `introsort` 之后对**整个范围**运行一次，而不是在每个子区间 <16 时分别运行。前者能利用 introsort 已将大元素排到后方的性质，只做少量移动。

4. **median_of_three 覆盖所有 6 种排列**：三个值的相对顺序有 6 种，代码需要覆盖全部；原始实现有时遗漏等号情况（a==b 时行为未定义）。

---

## 与 std 的对比和差异

| 功能 | mystl | std | 差异 |
|------|-------|-----|------|
| sort | introsort | introsort (pdqsort in libstdc++11+) | std 现代实现用 pdqsort，对近似有序更优 |
| stable_sort | 未实现 | timsort/merge sort | mystl 缺少稳定排序 |
| copy | memmove (trivial) | __builtin_memmove | 实现等价 |
| inplace_merge | 左半缓冲区 O(n) 空间 | 有缓冲区用归并，无缓冲区用旋转 O(n log n) | std 能在 O(1) 空间降级 |
| lower_bound | 仅支持 ForwardIt | 同 | 接口相同 |
| 缺失算法 | - | transform, accumulate, for_each, remove_if, partition 等 | mystl 只实现了核心子集 |

---

## 小结

本阶段实现了一个完整的算法库，核心亮点如下：

1. **introsort** 是工业级排序的真实形态：快排提供平均性能，堆排序提供最坏保证，插入排序消除小分区开销。三者缺一不可。

2. **memmove / memset 优化**：通过 `std::is_trivially_copy_assignable` 在编译期分流，对 POD 类型零开销接入 C 库底层原语。

3. **inplace_merge 的缓冲区设计**：直接在原数组上归并会导致"double-pickup"数据损坏，必须先将一半数据移走，隔离读写域。

4. **二分查找的统一抽象**：三个函数（lower_bound、upper_bound、binary_search）都依赖 `distance` + `next`，支持非随机访问迭代器，与标准库语义一致。

5. **next_permutation 的四步法**：找下降点 → 找交换点 → 交换 → 反转后缀，是一个经典但不显眼的算法，每次调用 O(n) 时间。
