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

## 八、运行测试

```bash
cmake --build build --target test_iterator -j$(nproc)
./build/phase02_iterator/test_iterator
```

预期输出：15 tests, 15 passed。
