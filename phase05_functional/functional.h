#pragma once
// functional.h — 函数对象与适配器（C++11 风格）
//
// 内容：
//   基本函数对象：plus, minus, multiplies, divides, modulus, negate
//                 equal_to, not_equal_to, less, greater, less_equal, greater_equal
//   逻辑函数对象：logical_and, logical_or, logical_not
//
//   适配器：
//     not1 / not2   — 对一元/二元函数对象取反（C++98 风格，C++17 已弃用）
//     bind1st / bind2nd — 绑定第一/第二参数（C++98 风格）
//     ptr_fun       — 将普通函数包装成函数对象
//     mem_fn        — 将成员函数包装成函数对象（简化版）
//
//   C++11 现代版对比（注释形式展示）

#include <functional>  // 仅用于基础 void_t 等，不依赖 std 的仿函数

namespace mystl {

// ============================================================
// 基本算术仿函数
// ============================================================

template <typename T>
struct plus {
    T operator()(const T& a, const T& b) const { return a + b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
};

template <typename T>
struct minus {
    T operator()(const T& a, const T& b) const { return a - b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
};

template <typename T>
struct multiplies {
    T operator()(const T& a, const T& b) const { return a * b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
};

template <typename T>
struct divides {
    T operator()(const T& a, const T& b) const { return a / b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
};

template <typename T>
struct modulus {
    T operator()(const T& a, const T& b) const { return a % b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
};

template <typename T>
struct negate {
    T operator()(const T& a) const { return -a; }
    using argument_type = T;
    using result_type   = T;
};

// ============================================================
// 比较仿函数
// ============================================================

template <typename T>
struct equal_to {
    bool operator()(const T& a, const T& b) const { return a == b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

template <typename T>
struct not_equal_to {
    bool operator()(const T& a, const T& b) const { return a != b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

template <typename T>
struct less {
    bool operator()(const T& a, const T& b) const { return a < b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

template <typename T>
struct greater {
    bool operator()(const T& a, const T& b) const { return a > b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

template <typename T>
struct less_equal {
    bool operator()(const T& a, const T& b) const { return a <= b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

template <typename T>
struct greater_equal {
    bool operator()(const T& a, const T& b) const { return a >= b; }
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = bool;
};

// ============================================================
// 逻辑仿函数
// ============================================================

template <typename T>
struct logical_and {
    bool operator()(const T& a, const T& b) const { return a && b; }
};

template <typename T>
struct logical_or {
    bool operator()(const T& a, const T& b) const { return a || b; }
};

template <typename T>
struct logical_not {
    bool operator()(const T& a) const { return !a; }
};

// ============================================================
// 适配器基类（C++98 风格，为 not1/not2/bind1st/bind2nd 提供类型信息）
// ============================================================

// 一元函数对象的基类（提供 argument_type 和 result_type）
template <typename Arg, typename Result>
struct unary_function {
    using argument_type = Arg;
    using result_type   = Result;
};

// 二元函数对象的基类
template <typename Arg1, typename Arg2, typename Result>
struct binary_function {
    using first_argument_type  = Arg1;
    using second_argument_type = Arg2;
    using result_type          = Result;
};

// ============================================================
// not1 / not2 — 对函数对象取反
// ============================================================

// unary_negate：包装一个一元谓词，对其结果取反
template <typename Predicate>
class unary_negate
    : public unary_function<
          typename Predicate::argument_type, bool>
{
public:
    explicit unary_negate(const Predicate& pred) : pred_(pred) {}
    bool operator()(const typename Predicate::argument_type& x) const {
        return !pred_(x);
    }
private:
    Predicate pred_;
};

template <typename Predicate>
unary_negate<Predicate> not1(const Predicate& pred) {
    return unary_negate<Predicate>(pred);
}

// binary_negate：包装一个二元谓词，对其结果取反
template <typename Predicate>
class binary_negate
    : public binary_function<
          typename Predicate::first_argument_type,
          typename Predicate::second_argument_type, bool>
{
public:
    explicit binary_negate(const Predicate& pred) : pred_(pred) {}
    bool operator()(const typename Predicate::first_argument_type& x,
                    const typename Predicate::second_argument_type& y) const {
        return !pred_(x, y);
    }
private:
    Predicate pred_;
};

template <typename Predicate>
binary_negate<Predicate> not2(const Predicate& pred) {
    return binary_negate<Predicate>(pred);
}

// ============================================================
// bind1st / bind2nd — 绑定参数
// ============================================================

// binder1st：绑定二元函数的第一个参数
template <typename BinaryOp>
class binder1st
    : public unary_function<
          typename BinaryOp::second_argument_type,
          typename BinaryOp::result_type>
{
public:
    binder1st(const BinaryOp& op,
              const typename BinaryOp::first_argument_type& val)
        : op_(op), val_(val) {}

    typename BinaryOp::result_type
    operator()(const typename BinaryOp::second_argument_type& x) const {
        return op_(val_, x);
    }
private:
    BinaryOp op_;
    typename BinaryOp::first_argument_type val_;
};

template <typename BinaryOp, typename T>
binder1st<BinaryOp> bind1st(const BinaryOp& op, const T& val) {
    return binder1st<BinaryOp>(op,
        typename BinaryOp::first_argument_type(val));
}

// binder2nd：绑定二元函数的第二个参数
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
        return op_(x, val_);
    }
private:
    BinaryOp op_;
    typename BinaryOp::second_argument_type val_;
};

template <typename BinaryOp, typename T>
binder2nd<BinaryOp> bind2nd(const BinaryOp& op, const T& val) {
    return binder2nd<BinaryOp>(op,
        typename BinaryOp::second_argument_type(val));
}

// ============================================================
// ptr_fun — 将普通函数包装成函数对象
// ============================================================

// 一元函数包装
template <typename Arg, typename Result>
class pointer_to_unary_function
    : public unary_function<Arg, Result>
{
public:
    explicit pointer_to_unary_function(Result (*f)(Arg)) : f_(f) {}
    Result operator()(Arg x) const { return f_(x); }
private:
    Result (*f_)(Arg);
};

template <typename Arg, typename Result>
pointer_to_unary_function<Arg, Result>
ptr_fun(Result (*f)(Arg)) {
    return pointer_to_unary_function<Arg, Result>(f);
}

// 二元函数包装
template <typename Arg1, typename Arg2, typename Result>
class pointer_to_binary_function
    : public binary_function<Arg1, Arg2, Result>
{
public:
    explicit pointer_to_binary_function(Result (*f)(Arg1, Arg2)) : f_(f) {}
    Result operator()(Arg1 x, Arg2 y) const { return f_(x, y); }
private:
    Result (*f_)(Arg1, Arg2);
};

template <typename Arg1, typename Arg2, typename Result>
pointer_to_binary_function<Arg1, Arg2, Result>
ptr_fun(Result (*f)(Arg1, Arg2)) {
    return pointer_to_binary_function<Arg1, Arg2, Result>(f);
}

// ============================================================
// C++11 现代写法对比（通过 lambda 或 std::function）
// ============================================================
// C++98 风格：
//   find_if(v.begin(), v.end(), bind2nd(greater<int>{}, 5))
//
// C++11 等价：
//   find_if(v.begin(), v.end(), [](int x){ return x > 5; })
//   find_if(v.begin(), v.end(), std::bind(greater<int>{}, _1, 5))

} // namespace mystl
