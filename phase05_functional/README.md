# Phase 05 — 函数对象与适配器 (`functional.h`)

> 本篇是 mini_stl 系列的第五篇。前几篇我们实现了内存分配器、迭代器、容器与算法，这一篇专注于 **函数对象（仿函数）** 与围绕它们建立的一套 **适配器体系**。
>
> 对应文件：`phase05_functional/functional.h`
> 命名空间：`mystl`

---

## 目录

1. [什么是函数对象（仿函数）](#1-什么是函数对象仿函数)
2. [算术仿函数](#2-算术仿函数)
3. [比较仿函数](#3-比较仿函数)
4. [逻辑仿函数](#4-逻辑仿函数)
5. [适配器基类：unary_function / binary_function](#5-适配器基类unary_function--binary_function)
6. [not1 / not2 — 谓词取反](#6-not1--not2--谓词取反)
7. [bind1st / bind2nd — 参数绑定](#7-bind1st--bind2nd--参数绑定)
8. [ptr_fun — 包装普通函数指针](#8-ptr_fun--包装普通函数指针)
9. [C++11 vs C++98 对比](#9-c11-vs-c98-对比)

---

## 1. 什么是函数对象（仿函数）

### 1.1 基本概念

**函数对象**（Function Object），又称 **仿函数**（Functor），是一个重载了 `operator()` 的类实例。任何可以像函数一样被调用的对象，都是函数对象。

```cpp
struct Adder {
    int operator()(int a, int b) const { return a + b; }
};

Adder add;
int result = add(3, 4);  // 输出 7，语法和调用函数一模一样
```

这里 `add` 是一个普通对象，却可以像 `add(3, 4)` 这样调用，这就是仿函数的核心。

### 1.2 为什么要用仿函数而不是普通函数？

**（1）可以携带状态**

普通函数没有内部状态，而仿函数是类，可以在构造时保存数据：

```cpp
struct Multiplier {
    int factor;
    Multiplier(int f) : factor(f) {}
    int operator()(int x) const { return x * factor; }
};

Multiplier times3(3);
times3(10);  // 30
times3(7);   // 21
```

普通函数做不到这一点——你需要全局变量或者闭包（C++11 lambda）才能模拟。

**（2）可以被内联，性能更好**

当仿函数作为模板参数传入时，编译器在实例化时就知道确切的类型，可以直接内联 `operator()` 的调用，消除函数调用开销。相比之下，函数指针在编译期是不透明的，编译器通常不能内联。

```cpp
// 编译器看到 Pred = mystl::less<int>，可以完全内联比较操作
template <typename InputIt, typename Pred>
InputIt find_if(InputIt first, InputIt last, Pred pred) {
    while (first != last && !pred(*first)) ++first;
    return first;
}
```

**（3）可以作为模板类型参数**

这是函数指针做不到的关键特性：

```cpp
// 函数指针版本：类型是"值"，不是"类型参数"
std::sort(v.begin(), v.end(), my_compare_fn);  // 传值

// 仿函数版本：可以把类型直接写进模板参数
std::set<int, mystl::greater<int>> s;  // greater<int> 是类型！
std::priority_queue<int, std::vector<int>, mystl::less<int>> pq;
```

容器如 `std::set`、`std::map`、`std::priority_queue` 都把比较器作为模板类型参数，这要求比较器是一个类型（仿函数类），而非一个运行时值（函数指针）。

---

## 2. 算术仿函数

`functional.h` 提供了六个算术仿函数，覆盖常见的四则运算：

| 仿函数 | 对应操作 | 元数 |
|--------|----------|------|
| `plus<T>` | `a + b` | 二元 |
| `minus<T>` | `a - b` | 二元 |
| `multiplies<T>` | `a * b` | 二元 |
| `divides<T>` | `a / b` | 二元 |
| `modulus<T>` | `a % b` | 二元 |
| `negate<T>` | `-a` | 一元 |

### 2.1 基本用法

```cpp
#include "functional.h"

mystl::plus<int>       add;
mystl::minus<int>      sub;
mystl::multiplies<double> mul;
mystl::divides<int>    div;
mystl::modulus<int>    mod;
mystl::negate<int>     neg;

add(3, 4);    // 7
sub(10, 3);   // 7
mul(2.5, 4.0); // 10.0
div(10, 3);   // 3（整数除法）
mod(10, 3);   // 1
neg(5);       // -5
```

### 2.2 关键细节：类型别名 (typedef)

每个二元算术仿函数都内置了三个类型别名：

```cpp
template <typename T>
struct plus {
    T operator()(const T& a, const T& b) const { return a + b; }

    using first_argument_type  = T;   // 第一个参数的类型
    using second_argument_type = T;   // 第二个参数的类型
    using result_type          = T;   // 返回值的类型
};
```

一元仿函数（`negate`）则有两个：

```cpp
template <typename T>
struct negate {
    T operator()(const T& a) const { return -a; }

    using argument_type = T;   // 参数类型
    using result_type   = T;   // 返回值类型
};
```

**这些别名为什么重要？**

适配器（`not1`、`not2`、`bind1st`、`bind2nd`）需要在编译期知道仿函数的参数类型和返回值类型，才能自己声明正确的 `operator()` 签名。它们通过这些 `using` 别名来获取这些信息。如果一个仿函数没有这些别名，就无法被这些适配器包装。

---

## 3. 比较仿函数

提供六个比较仿函数，返回值均为 `bool`：

| 仿函数 | 对应操作 |
|--------|----------|
| `equal_to<T>` | `a == b` |
| `not_equal_to<T>` | `a != b` |
| `less<T>` | `a < b` |
| `greater<T>` | `a > b` |
| `less_equal<T>` | `a <= b` |
| `greater_equal<T>` | `a >= b` |

### 3.1 直接用于排序

最常见的用法是作为 `mystl::sort` 的第三个参数，改变排序方向：

```cpp
#include "functional.h"
#include "../phase04_algorithm/algorithm.h"
#include "../phase03_containers/vector.h"

mystl::vector<int> v = {5, 3, 8, 1, 7};

// 升序排序（默认行为等价于 less<int>）
mystl::sort(v.begin(), v.end(), mystl::less<int>{});
// v: [1, 3, 5, 7, 8]

// 降序排序
mystl::sort(v.begin(), v.end(), mystl::greater<int>{});
// v: [8, 7, 5, 3, 1]
```

注意这里的 `mystl::greater<int>{}` 是用花括号构造了一个临时对象（aggregate 初始化），这是 C++11 的写法。等价于 `mystl::greater<int>()`。

### 3.2 与字符串比较

仿函数是模板类，可以用于任何支持对应运算符的类型：

```cpp
mystl::equal_to<std::string> eq;
eq("hello", "hello");  // true
eq("hello", "world");  // false
```

### 3.3 作为关联容器的比较器类型

```cpp
// 自定义大根堆（每次 top() 返回最大元素）
// less<int> 意味着"父节点 >= 子节点"（标准堆行为）
std::priority_queue<int, std::vector<int>, mystl::less<int>> max_heap;

// 小根堆
std::priority_queue<int, std::vector<int>, mystl::greater<int>> min_heap;
```

---

## 4. 逻辑仿函数

三个逻辑仿函数，对应 `&&`、`||`、`!`：

```cpp
mystl::logical_and<bool> land;
mystl::logical_or<bool>  lor;
mystl::logical_not<bool> lnot;

land(true, true);   // true
land(true, false);  // false
lor(false, true);   // true
lnot(false);        // true
lnot(true);         // false
```

注意在 `functional.h` 的实现中，`logical_and` 和 `logical_or` 没有添加 `first_argument_type` 等别名，这是故意的——它们通常不需要被适配器包装，因为逻辑操作基本上配合 lambda 使用更自然。

### 4.1 与算法配合

```cpp
// 判断两个条件都满足
mystl::logical_and<bool> both;
bool a = some_condition(), b = another_condition();
if (both(a, b)) { /* ... */ }
```

---

## 5. 适配器基类：`unary_function` / `binary_function`

### 5.1 为什么需要基类？

C++98 的适配器体系（`not1`/`not2`/`bind1st`/`bind2nd`）工作原理是：通过读取被包装仿函数的 `argument_type`、`result_type` 等类型别名，来推导自身的 `operator()` 签名。

为了让自定义仿函数也能被这些适配器使用，标准提供了两个辅助基类，只需继承它们，就自动获得这些别名：

```cpp
// 一元函数基类：提供 argument_type 和 result_type
template <typename Arg, typename Result>
struct unary_function {
    using argument_type = Arg;
    using result_type   = Result;
};

// 二元函数基类：提供 first/second_argument_type 和 result_type
template <typename Arg1, typename Arg2, typename Result>
struct binary_function {
    using first_argument_type  = Arg1;
    using second_argument_type = Arg2;
    using result_type          = Result;
};
```

### 5.2 如何使用

定义自己的仿函数时，继承对应的基类：

```cpp
// 不继承基类的写法——无法被 not1 包装
struct is_even_bad {
    bool operator()(int x) const { return x % 2 == 0; }
    // 没有 argument_type，not1 无法使用
};

// 正确写法：继承 unary_function
struct is_even : mystl::unary_function<int, bool> {
    bool operator()(int x) const { return x % 2 == 0; }
    // 自动拥有 argument_type = int, result_type = bool
};

// 现在 not1 可以包装它
auto is_odd = mystl::not1(is_even{});
```

这个设计思路是 C++98/03 时代"通过继承注入类型信息"的典型模式。到了 C++11，`std::result_of`、模板特化等机制让这个基类变得不再必要，C++17 正式将其弃用。

---

## 6. `not1` / `not2` — 谓词取反

### 6.1 原理

`not1` 和 `not2` 是两个工厂函数，分别创建 `unary_negate` 和 `binary_negate` 对象。这两个类在内部保存原谓词，在 `operator()` 中调用原谓词并取反其结果。

```
not1(pred)(x)    等价于    !pred(x)
not2(pred)(x, y) 等价于    !pred(x, y)
```

**实现核心**（`unary_negate`）：

```cpp
template <typename Predicate>
class unary_negate
    : public unary_function<typename Predicate::argument_type, bool>
{
public:
    explicit unary_negate(const Predicate& pred) : pred_(pred) {}
    bool operator()(const typename Predicate::argument_type& x) const {
        return !pred_(x);  // 调用原谓词，取反
    }
private:
    Predicate pred_;
};
```

注意 `unary_negate` 通过 `Predicate::argument_type` 来确定自己的参数类型——这就是为什么被包装的谓词必须有 `argument_type` 别名（即继承了 `unary_function`）。

### 6.2 not1 示例：找第一个奇数

```cpp
#include "functional.h"
#include "../phase04_algorithm/algorithm.h"

// 定义"是偶数"谓词（必须继承 unary_function 以提供 argument_type）
struct is_even : mystl::unary_function<int, bool> {
    bool operator()(int x) const { return x % 2 == 0; }
};

int arr[] = {2, 4, 1, 6, 3};

// not1(is_even{}) 等价于 "不是偶数"，即"是奇数"
auto it = mystl::find_if(arr, arr + 5, mystl::not1(is_even{}));
// *it == 1（第一个奇数）
```

### 6.3 not2 示例：等价于 greater_equal

`not2(less<int>{})` 对 `less` 取反，得到 `!(a < b)`，即 `a >= b`，等价于 `greater_equal<int>{}`：

```cpp
auto not_less = mystl::not2(mystl::less<int>{});

not_less(5, 3);   // !(5 < 3) = !false = true   → 5 >= 3 ✓
not_less(3, 3);   // !(3 < 3) = !false = true   → 3 >= 3 ✓
not_less(2, 5);   // !(2 < 5) = !true  = false  → 2 >= 5 ✗
```

这是一种利用已有仿函数组合出新语义的方式。

### 6.4 前提条件总结

| 适配器 | 要求被包装的谓词拥有 |
|--------|----------------------|
| `not1` | `argument_type` |
| `not2` | `first_argument_type`、`second_argument_type` |

---

## 7. `bind1st` / `bind2nd` — 参数绑定

### 7.1 概念

`bind2nd(op, val)` 将二元函数 `op` 的**第二个参数**固定为 `val`，返回一个一元函数对象，调用时只需提供第一个参数：

```
bind2nd(op, val)(x)  等价于  op(x, val)
bind1st(op, val)(x)  等价于  op(val, x)
```

### 7.2 binder2nd 实现原理

```cpp
template <typename BinaryOp>
class binder2nd
    : public unary_function<
          typename BinaryOp::first_argument_type,
          typename BinaryOp::result_type>
{
public:
    binder2nd(const BinaryOp& op,
              const typename BinaryOp::second_argument_type& val)
        : op_(op), val_(val) {}

    typename BinaryOp::result_type
    operator()(const typename BinaryOp::first_argument_type& x) const {
        return op_(x, val_);  // 第一个参数由调用者提供，第二个固定为 val_
    }
private:
    BinaryOp op_;
    typename BinaryOp::second_argument_type val_;
};
```

`binder2nd` 本身也继承 `unary_function`，因此**它产生的对象也可以被 `not1` 进一步包装**，形成适配器链。

### 7.3 bind2nd 示例：筛选大于 5 的元素

```cpp
int arr[] = {1, 3, 5, 7, 9};

// bind2nd(greater<int>{}, 5) 固定了 greater 的第二个参数为 5
// 等价于：对每个 x，计算 x > 5
auto gt5 = mystl::bind2nd(mystl::greater<int>{}, 5);

auto it = mystl::find_if(arr, arr + 5, gt5);
// *it == 7（第一个大于 5 的元素）
```

还可以用于 `count_if`：

```cpp
int arr[] = {5, 10, 15, 20, 3};
auto gt10 = mystl::bind2nd(mystl::greater<int>{}, 10);
auto n = mystl::count_if(arr, arr + 5, gt10);
// n == 2（15 和 20）
```

### 7.4 bind1st 示例：固定乘数

`bind1st(op, val)` 固定**第一个**参数：

```cpp
// bind1st(multiplies<int>{}, 3) 固定了乘法的第一个参数为 3
// 等价于：对每个 x，计算 3 * x
auto triple = mystl::bind1st(mystl::multiplies<int>{}, 3);

triple(4);   // 3 * 4 = 12
triple(7);   // 3 * 7 = 21
```

### 7.5 前提条件

`bind1st`/`bind2nd` 要求二元操作对象（`BinaryOp`）必须拥有：

- `first_argument_type`
- `second_argument_type`
- `result_type`

这就是为什么 `mystl` 中所有二元仿函数都声明了这三个别名——为了能够被这些适配器使用。

### 7.6 适配器链组合

`binder2nd` 本身继承了 `unary_function`（提供了 `argument_type`），所以 `bind2nd` 的结果可以直接被 `not1` 进一步包装：

```cpp
// "不大于 5" == "小于等于 5"
auto le5 = mystl::not1(mystl::bind2nd(mystl::greater<int>{}, 5));

le5(3);  // !( 3 > 5) = !(false) = true   → 3 <= 5 ✓
le5(7);  // !( 7 > 5) = !(true)  = false  → 7 <= 5 ✗
```

---

## 8. `ptr_fun` — 包装普通函数指针

### 8.1 问题：裸函数指针没有类型别名

`not1` 要求被包装的对象有 `argument_type`，`bind2nd` 要求有 `first_argument_type` 等。但**普通函数**只是一个函数指针，根本没有这些成员类型。

```cpp
bool is_positive(int x) { return x > 0; }

// 错误！is_positive 是函数指针，没有 argument_type
// auto pred = mystl::not1(is_positive);  // 编译失败

// 正确做法：用 ptr_fun 包装
auto pred = mystl::not1(mystl::ptr_fun(is_positive));  // OK
```

### 8.2 ptr_fun 的实现

`ptr_fun` 是一个工厂函数，根据函数指针的参数数量（一元/二元），自动创建 `pointer_to_unary_function` 或 `pointer_to_binary_function` 对象。这两个包装类都继承了对应的基类，从而获得必要的类型别名。

**一元包装**：

```cpp
template <typename Arg, typename Result>
class pointer_to_unary_function
    : public unary_function<Arg, Result>   // 自动获得 argument_type, result_type
{
public:
    explicit pointer_to_unary_function(Result (*f)(Arg)) : f_(f) {}
    Result operator()(Arg x) const { return f_(x); }  // 调用原函数
private:
    Result (*f_)(Arg);  // 保存函数指针
};

template <typename Arg, typename Result>
pointer_to_unary_function<Arg, Result> ptr_fun(Result (*f)(Arg)) {
    return pointer_to_unary_function<Arg, Result>(f);
}
```

**二元包装**类似，继承 `binary_function` 从而获得 `first/second_argument_type`。

### 8.3 完整示例

```cpp
static bool is_positive(int x) { return x > 0; }
static int  add_ten(int x)     { return x + 10; }

int arr[] = {1, -2, 3, -4, 5};

// ptr_fun 包装 is_positive，使其拥有 argument_type
auto pred = mystl::ptr_fun(is_positive);

// 找第一个正数
auto it = mystl::find_if(arr, arr + 5, pred);
// *it == 1

// 配合 not1：找第一个非正数（<= 0 的数）
auto not_positive = mystl::not1(mystl::ptr_fun(is_positive));
auto it2 = mystl::find_if(arr, arr + 5, not_positive);
// *it2 == -2
```

如果不用 `ptr_fun`，直接 `not1(is_positive)` 会在编译期报错，因为裸函数指针类型 `bool(*)(int)` 没有 `argument_type` 成员。

### 8.4 ptr_fun 的两个重载

编译器根据你传入的函数指针类型（一元或二元）自动选择正确的 `ptr_fun` 重载：

```cpp
// 一元：ptr_fun(Result(*)(Arg))
auto f1 = mystl::ptr_fun(is_positive);   // pointer_to_unary_function<int, bool>

// 二元：ptr_fun(Result(*)(Arg1, Arg2))
static int add(int a, int b) { return a + b; }
auto f2 = mystl::ptr_fun(add);            // pointer_to_binary_function<int, int, int>
```

---

## 9. C++11 vs C++98 对比

### 9.1 历史背景

上面介绍的 `bind1st`/`bind2nd`/`not1`/`not2`/`ptr_fun` 是 C++98 标准库的函数适配器体系。这套体系有明显的局限性：

- 使用繁琐，必须通过继承基类或手写 typedef 来满足适配器的要求
- 只支持一元和二元函数，三元以上无法适配
- 无法方便地组合多个操作

C++11 引入了 **lambda 表达式** 和增强的 **`std::bind`**，使上述适配器变得几乎没有必要。C++11 将它们标记为 **deprecated（弃用）**，C++17 彻底 **removed（移除）** 了 `bind1st`/`bind2nd`/`not1`/`not2`/`ptr_fun`（以及 `unary_function`/`binary_function` 基类）。

### 9.2 等价写法对照表

| C++98 写法 | C++11 等价写法 |
|------------|----------------|
| `bind2nd(greater<int>{}, 5)` | `[](int x){ return x > 5; }` |
| `bind1st(multiplies<int>{}, 3)` | `[](int x){ return 3 * x; }` |
| `not1(is_even{})` | `[](int x){ return x % 2 != 0; }` |
| `not1(ptr_fun(is_positive))` | `[](int x){ return !is_positive(x); }` |
| `not2(less<int>{})` | `[](int a, int b){ return !(a < b); }` |

### 9.3 std::bind 与占位符

C++11 的 `std::bind` 更灵活，通过占位符 `_1`、`_2` 指定哪些参数由调用者提供：

```cpp
#include <functional>
using namespace std::placeholders;

// bind2nd(greater<int>{}, 5) 的现代等价写法
// _1 表示"调用时第一个参数"，5 是固定值
auto gt5 = std::bind(mystl::greater<int>{}, _1, 5);
gt5(7);   // greater<int>{}(7, 5) → 7 > 5 → true
gt5(3);   // greater<int>{}(3, 5) → 3 > 5 → false

// bind1st(multiplies<int>{}, 3) 的现代等价写法
auto triple = std::bind(mystl::multiplies<int>{}, 3, _1);
triple(4);  // multiplies<int>{}(3, 4) → 12
```

### 9.4 实际建议

| 场景 | C++11 推荐做法 |
|------|---------------|
| 简单条件判断 | lambda 表达式（最清晰） |
| 复用已有仿函数并绑定参数 | `std::bind` + 占位符 |
| 需要将函数对象存储在变量中 | `std::function<R(Args...)>` |
| 谓词取反 | C++17 `std::not_fn`，或直接 lambda |

```cpp
// 最现代的写法：lambda 最直观
mystl::vector<int> v = {1, 3, 5, 7, 9};

// 找第一个 > 5 的元素
auto it = mystl::find_if(v.begin(), v.end(),
    [](int x) { return x > 5; });

// 统计奇数个数
auto n = mystl::count_if(v.begin(), v.end(),
    [](int x) { return x % 2 != 0; });

// 降序排序
mystl::sort(v.begin(), v.end(),
    [](int a, int b) { return a > b; });
// 或者更简洁地用仿函数：
mystl::sort(v.begin(), v.end(), mystl::greater<int>{});
```

### 9.5 那我们为什么还要实现这些旧适配器？

即使在现代 C++ 中这些适配器已被弃用，实现它们仍然有很大的教学价值：

1. **理解"类型萃取"的思想**：通过类型别名在编译期传递类型信息，是 C++ 模板元编程的基础思想。
2. **理解适配器模式**：`not1`/`bind2nd` 是"装饰器/适配器"设计模式在类型系统层面的体现。
3. **阅读旧代码的能力**：大量遗留 C++ 代码库中还在使用这些适配器，理解它们是读懂老代码的前提。
4. **更深入理解 lambda**：当你知道 `bind2nd(greater<int>{}, 5)` 展开后是什么，你才能更好地理解 lambda 替代它时省了多少心智负担。

---

## 总结

本篇实现的内容可以用一张图来概括：

```
                    ┌─────────────────────────────────────┐
                    │          函数对象体系                │
                    └─────────────────────────────────────┘
                                     │
          ┌──────────────────────────┼──────────────────────────┐
          │                          │                          │
    算术仿函数                  比较仿函数                  逻辑仿函数
 plus/minus/...            less/greater/...          logical_and/or/not
 (携带 first/second_       (携带 first/second_
  argument_type,            argument_type,
  result_type)              result_type=bool)
          │                          │
          └──────────┬───────────────┘
                     │
              适配器基类
     unary_function / binary_function
     （通过继承注入类型别名）
                     │
          ┌──────────┼──────────────┐
          │          │              │
        not1/not2  bind1st/bind2nd  ptr_fun
       (谓词取反) (参数绑定)    (包装函数指针)
```

关键设计思想：

- 仿函数通过 **类型别名**（`argument_type` 等）向适配器暴露自己的接口信息
- 适配器通过**读取这些别名**在编译期推导自身的类型签名，形成可组合的类型安全的函数组合链
- C++11 的 lambda 从根本上解决了这套体系的复杂性问题，但理解旧体系是掌握现代 C++ 的重要一步

---

## 复杂度分析表

| 组件 | 调用开销 | 编译期开销 | 备注 |
|------|---------|-----------|------|
| plus/minus 等算术仿函数 | O(1) | 零 | 等价于内联运算符 |
| less/greater 等比较仿函数 | O(1) | 零 | 等价于 </> |
| not1(pred)(x) | O(1) | 一层包装 | 编译器通常完全内联 |
| not2(pred)(x,y) | O(1) | 一层包装 | 同上 |
| bind2nd(op,val)(x) | O(1) | 一层包装 | 每次调用固定第二参数 |
| bind1st(op,val)(x) | O(1) | 一层包装 | 每次调用固定第一参数 |
| ptr_fun(f)(x) | O(1) | 一层包装 | 包装函数指针，无虚调用 |
| not1(ptr_fun(f))(x) | O(1) | 两层包装 | 两次 operator() 调用，均可内联 |

注：所有仿函数和适配器均为 `const` 成员函数 + 值语义，编译器能充分内联优化。

---

## 代码走读：not1(ptr_fun(is_positive)) 展开过程

逐步追踪类型链的展开：

```
1. is_positive 是 bool(*)(int)，没有 argument_type

2. ptr_fun(is_positive)
   → pointer_to_unary_function<int, bool>
   → 有 argument_type = int, result_type = bool
   → operator()(int x) { return is_positive(x); }

3. not1(ptr_fun(is_positive))
   → unary_negate<pointer_to_unary_function<int,bool>>
   → 继承自 unary_function<int, bool>
   → operator()(int x) { return !pred_(x); }
                              = !is_positive(x)

4. find_if(arr, arr+4, not_positive)
   → 对每个元素调用 not_positive(*it)
   → 展开为 !is_positive(*it)
   → 找第一个非正数
```

对比直接用裸函数指针为何失败：

```
not1(is_positive)
  → unary_negate<bool(*)(int)>
  → typename Predicate::argument_type  ← 编译错误！
     函数指针类型没有 ::argument_type
```

因此必须先经过 `ptr_fun` 包装，才能为适配器提供所需的类型别名。

---

## 实现难点 & 踩坑记录

1. **not1 要求 argument_type**：`unary_negate<Pred>` 在声明时就萃取 `Pred::argument_type`，如果 `Pred` 是普通函数或没有继承 `unary_function` 的仿函数，直接编译报错。必须用 `ptr_fun` 包装或继承 `unary_function`。

2. **bind1st/bind2nd 要求完整的三个类型别名**：需要 `first_argument_type`、`second_argument_type`、`result_type` 三者都有。缺少任何一个都会导致编译失败。

3. **mystl::less 和 std::less 的 first_argument_type**：C++17 中 `std::less` 移除了这些 typedef（改用透明比较符），所以 `bind2nd(std::less<int>{}, 5)` 在 C++17 中编译失败。本实现用自定义 `mystl::less` 保留了这些 typedef。

4. **not2 的类型转发**：`binary_negate::operator()` 的参数类型必须精确匹配 `Predicate::first/second_argument_type`，如果谓词用 `const T&` 而适配器用 `T`，会发生隐式拷贝，但仍能工作；类型不兼容时则报错。

---

## 与 std 的对比和差异

| C++98 mystl | std 等价（C++11+） | C++17 状态 | 说明 |
|------------|-----------------|-----------|------|
| not1(pred) | !pred(x) lambda 或 std::not_fn(pred) | 弃用 not1 | std::not_fn 不要求 argument_type |
| not2(pred) | std::not_fn(pred) | 弃用 not2 | 同上 |
| bind1st(f,v) | [v](auto x){return f(v,x);} 或 std::bind(f,v,_1) | 弃用 bind1st | std::bind 不要求类型别名 |
| bind2nd(f,v) | [v](auto x){return f(x,v);} 或 std::bind(f,_1,v) | 弃用 bind2nd | 同上 |
| ptr_fun(f) | 直接用 f（lambda 可包装） | 弃用 ptr_fun | C++11 lambda 解决了所有场景 |
| unary_function | 无直接替代 | 弃用 | C++20 用 concepts |
| binary_function | 无直接替代 | 弃用 | C++20 用 concepts |

本实现忠实还原 C++98/03 风格，是理解 STL 历史演化的最好切入点。现代 C++ 已用 lambda + std::bind + std::not_fn + concepts 彻底替代了这套体系。
