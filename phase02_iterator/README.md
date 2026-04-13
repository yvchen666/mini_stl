# Phase 02 — 迭代器与 traits 萃取

> **目标**：理解为什么 STL 算法能统一处理原生指针和自定义迭代器，以及这套设计背后的机制。

---

## 一、问题起源

STL 的算法（如 `distance`、`advance`）想对所有迭代器类型用同一套接口：

```cpp
auto d = mystl::distance(first, last);
mystl::advance(it, 5);
```

但这立刻遇到两个问题：

**问题 1**：原生指针没有成员类型
```cpp
int* p;
// p::value_type  ← 编译错误！指针没有嵌套 typedef
```

**问题 2**：不同类别的迭代器效率不同
- `random_access` 的 `distance` = O(1)（`last - first`）
- `bidirectional` 的 `distance` = O(n)（逐步计数）

如果只写一个版本，必然牺牲一方。

---

## 二、解法：iterator_traits 萃取机

通过**模板特化**打补丁：

```
iterator_traits<T>        → 主模板：读取类的 nested typedef
iterator_traits<T*>       → 偏特化：硬编码 random_access，value_type = T
iterator_traits<const T*> → 偏特化：同上，value_type 仍为 T（非 const）
```

统一的访问语法：
```cpp
typename mystl::iterator_traits<It>::value_type        // 元素类型
typename mystl::iterator_traits<It>::iterator_category  // 分类标签
typename mystl::iterator_traits<It>::difference_type   // 距离类型
```

---

## 三、五种迭代器标签的继承体系

```
std::input_iterator_tag
  └─ mystl::input_iterator_tag
        └─ mystl::forward_iterator_tag
              └─ mystl::bidirectional_iterator_tag
                    └─ mystl::random_access_iterator_tag
```

`mystl` 的 tag 继承自 `std` 的对应 tag，实现**双向兼容**：
- STL 算法（接受 `std::input_iterator_tag`）能处理 mystl 迭代器
- mystl 算法（接受 `std::input_iterator_tag`）也能处理 STL 容器的迭代器

---

## 四、tag dispatch — 编译期函数分发

`advance` 的三个重载：

```cpp
// 只能前进，n >= 0
void advance_aux(InputIt& it, Distance n, std::input_iterator_tag);

// 可前可后
void advance_aux(BidirIt& it, Distance n, std::bidirectional_iterator_tag);

// O(1)
void advance_aux(RandomIt& it, Distance n, std::random_access_iterator_tag);

// 统一入口：编译期根据 iterator_category 选择重载
template <typename InputIt, typename Distance>
void advance(InputIt& it, Distance n) {
    advance_aux(it, n,
        typename std::iterator_traits<InputIt>::iterator_category{});
        //                                                          ↑ 构造一个临时 tag 对象
        //                                     ↑ 编译期萃取 → 选择正确的重载
}
```

这个空的 `{}` 构造的 tag 对象仅用于重载决议，**运行时零开销**。

---

## 五、reverse_iterator 的关键设计

### 为什么 `operator*` 要"退一步"？

`rbegin()` 对应 `end()`，直接解引用 `end()` 是未定义行为。

```
正向：[1] [2] [3] [4] [5]  end
                            ↑ rbegin.base()

*rbegin = *(end - 1) = 5   ✓
```

实现：
```cpp
reference operator*() const {
    Iterator tmp = current_;
    return *--tmp;  // 先退一步，再解引用
}
```

### 比较运算符的方向反转

```cpp
// rev_a < rev_b  等价于  base_a > base_b
template <typename It1, typename It2>
bool operator<(const reverse_iterator<It1>& a,
               const reverse_iterator<It2>& b) {
    return b.base() < a.base();  // 注意：b 和 a 互换了
}
```

因为 base 值越大，在逻辑上越靠前（越接近 rbegin）。

---

## 六、自定义迭代器示例

```cpp
struct MyForwardIt
    : mystl::iterator<mystl::forward_iterator_tag, int>
{
    int* ptr;
    explicit MyForwardIt(int* p) : ptr(p) {}

    bool operator==(const MyForwardIt& o) const { return ptr == o.ptr; }
    bool operator!=(const MyForwardIt& o) const { return ptr != o.ptr; }
    int& operator*()  const { return *ptr; }
    int* operator->() const { return ptr; }
    MyForwardIt& operator++() { ++ptr; return *this; }
    MyForwardIt  operator++(int) { auto t = *this; ++ptr; return t; }
};

// 现在 mystl::distance / mystl::advance 都能正确处理 MyForwardIt
```

---

## 七、常见问题

**Q: 为什么 `const T*` 的 `value_type` 是 `T` 而不是 `const T`？**

`value_type` 表示"元素本身的类型"，不包含 cv 限定符。`const` 是访问限制，不是类型的一部分。这和 `std::remove_const` 的语义一致。

**Q: iterator_traits 没有它会发生什么？**

算法无法处理原生指针（`int*`, `double*`），因为指针没有 `::value_type`、`::iterator_category` 等成员类型。整个 `<algorithm>` 就无法对数组使用。

**Q: 为什么 mystl 的 tag 要继承 std 的 tag？**

只要 mystl 的 tag 不继承 std 的，`std::sort`、`std::copy` 等标准算法就无法处理 mystl 容器的迭代器（因为标准算法内部的 tag dispatch 匹配不上）。继承后自动兼容。

---

## 八、复杂度分析表

| 操作 | 时间复杂度 | 空间复杂度 | 备注 |
|------|-----------|-----------|------|
| `distance` (random_access) | O(1) | O(1) | `last - first`，单次指针差 |
| `distance` (input/forward/bidir) | O(n) | O(1) | 逐步 `++it` 计数，n 为距离 |
| `advance` (random_access) | O(1) | O(1) | `it += n`，直接偏移 |
| `advance` (input) | O(n) | O(1) | 只能前进，循环 `++it` |
| `advance` (bidirectional) | O(n) | O(1) | n<0 时循环 `--it`，n>0 时循环 `++it` |
| `iterator_traits` 成员访问（任意类型） | O(1)（编译期） | O(0) | 纯 typedef 萃取，无运行时代价 |
| `reverse_iterator::operator*` | O(1) | O(1) | 拷贝 `current_`，退一步再解引用 |
| `reverse_iterator::operator++` | O(1) | O(1) | `--current_`，方向反转 |

---

## 九、代码走读（逐步图解）

### distance 对 vector<int> 迭代器的派发路径

```
vector<int> v = {10, 20, 30, 40, 50};  // size = 5
mystl::distance(v.begin(), v.end())

distance(it_begin, it_end)
 │
 └─ 萃取 iterator_category：
      iterator_traits<vector<int>::iterator>::iterator_category
        = random_access_iterator_tag
 │
 └─ 构造临时 tag 对象，调用重载：
      distance_aux(begin, end, random_access_iterator_tag{})
 │
 └─ return end - begin = 5        // O(1)，单次指针差运算
```

### distance 对 list<int> 迭代器的派发路径

```
list<int> lst = {1, 2, 3, 4, 5};
mystl::distance(lst.begin(), lst.end())

distance(it_begin, it_end)
 │
 └─ 萃取 iterator_category：
      iterator_traits<list<int>::iterator>::iterator_category
        = bidirectional_iterator_tag
 │
 └─ bidirectional_iterator_tag 继承自 input_iterator_tag
      → 匹配 distance_aux(..., input_iterator_tag) 重载
 │
 └─ 逐步前进计数：
      step 0: it → node(1)  count=0
      step 1: ++it → node(2)  count=1
      step 2: ++it → node(3)  count=2
      step 3: ++it → node(4)  count=3
      step 4: ++it → node(5)  count=4
      step 5: ++it → end      count=5
      return 5                 // O(n)，n=5 次 ++ 操作
```

---

## 十、实现难点 & 踩坑记录

### 1. mystl tag 不继承 std tag 的后果

若将 mystl 的 tag 定义为独立结构体（不继承 std tag）：

```cpp
// 错误示例：独立 tag
struct input_iterator_tag {};   // 不继承 std::input_iterator_tag

// std::copy 内部期待 std::input_iterator_tag，无法匹配 mystl tag
// → 编译错误或调用错误重载
std::copy(mylist.begin(), mylist.end(), dest);
```

正确做法是继承，使 mystl 迭代器对 `std` 算法透明：

```cpp
struct input_iterator_tag : std::input_iterator_tag {};
```

### 2. reverse_iterator::operator* 不能直接解引用 current_

`current_` 存储的是"逻辑当前元素的下一位"的底层迭代器。直接 `*current_` 实际解引用的是下一个元素，在 `current_ == end()` 时是未定义行为：

```cpp
// 错误写法：
reference operator*() const { return *current_; }  // UB when current_ == end()

// 正确写法：先退一步，再解引用
reference operator*() const {
    Iterator tmp = current_;
    return *--tmp;   // 副本回退，不修改 current_，符合 const 语义
}
```

`--tmp` 作用在副本上，`current_` 本身不变，符合 `const` 成员函数语义。

### 3. const_iterator 与 iterator 的互换性

`reverse_iterator<iterator>` 和 `reverse_iterator<const_iterator>` 是不同类型，需要显式提供转换构造函数，否则常见的 `const` 转换会编译失败：

```cpp
// 若无转换构造函数：
reverse_iterator<iterator> ri = v.rbegin();
reverse_iterator<const_iterator> cri = ri;   // 编译错误！

// 正确：提供模板转换构造函数
template <typename U>
reverse_iterator(const reverse_iterator<U>& other)
    : current_(other.base()) {}
```

`std::reverse_iterator` 通过 `requires std::convertible_to<U, Iterator>` 约束保证安全性；mystl 教学版需手动实现或用 `static_assert` 保护。

---

## 十一、与 std 的对比和差异

### mystl::iterator_traits vs std::iterator_traits 接口对比

| 成员类型 | mystl::iterator_traits | std::iterator_traits | 备注 |
|---------|----------------------|---------------------|------|
| `value_type` | 有 | 有 | 语义相同 |
| `difference_type` | 有 | 有 | 语义相同 |
| `pointer` | 有 | 有 | 语义相同 |
| `reference` | 有 | 有 | 语义相同 |
| `iterator_category` | 有 | 有 | 语义相同 |

### 关键区别

- **tag 继承方向**：mystl tags 继承 std tags（`mystl::input_iterator_tag : std::input_iterator_tag`），保证 mystl 迭代器能被 std 算法接受。反向不成立：std tags 不继承 mystl tags，std 容器的迭代器在 mystl 算法中靠继承链自动匹配。
- **mystl 版本是教学简化版**：不处理 `void` 类型特化、不提供 C++20 `contiguous_iterator_tag`、不实现 `iter_reference_t` 等辅助 traits。

### C++20 的变化

C++20 引入 Concepts，tag dispatch 逐渐被 Concept 约束取代：

```cpp
// C++17 tag dispatch 风格（mystl 采用）：
template <typename It>
void advance(It& it, int n) {
    advance_aux(it, n, typename iterator_traits<It>::iterator_category{});
}

// C++20 Concepts 风格（std::ranges 采用）：
template <std::random_access_iterator It>
void advance(It& it, int n) { it += n; }

template <std::input_iterator It>
void advance(It& it, int n) { while (n--) ++it; }
```

`std::ranges::distance` 使用 `std::sized_sentinel_for` concept 在编译期判断是否 O(1)，不再依赖 tag 对象传递。mystl 的 tag dispatch 实现忠于 C++98/03 的历史设计，有助于理解 concepts 出现前标准库的工作方式。

---

## 十二、运行测试

```bash
cmake --build build --target test_iterator -j$(nproc)
./build/phase02_iterator/test_iterator
```

预期输出：15 tests, 15 passed。
