#include <gtest/gtest.h>
#include "alloc.h"
#include "construct.h"
#include "uninitialized.h"

#include <string>
#include <vector>

// ============================================================
// construct / destroy
// ============================================================

TEST(Construct, DefaultConstruct) {
    char buf[sizeof(int)];
    int* p = reinterpret_cast<int*>(buf);
    mystl::construct(p);
    // 值初始化后为 0
    EXPECT_EQ(*p, 0);
    mystl::destroy(p);
}

TEST(Construct, CopyConstruct) {
    char buf[sizeof(std::string)];
    std::string* p = reinterpret_cast<std::string*>(buf);
    mystl::construct(p, std::string("hello"));
    EXPECT_EQ(*p, "hello");
    mystl::destroy(p);
}

TEST(Construct, EmplaceConstruct) {
    char buf[sizeof(std::string)];
    std::string* p = reinterpret_cast<std::string*>(buf);
    mystl::construct(p, 5, 'x');  // std::string(5, 'x') = "xxxxx"
    EXPECT_EQ(*p, "xxxxx");
    mystl::destroy(p);
}

TEST(Destroy, RangeDestroy_Trivial) {
    int arr[5] = {1, 2, 3, 4, 5};
    // trivially destructible：调用应为空操作，不崩溃即可
    mystl::destroy(arr, arr + 5);
    (void)arr;
}

TEST(Destroy, RangeDestroy_NonTrivial) {
    // 用 placement new 构造一批 string，然后批量析构
    constexpr int N = 4;
    alignas(std::string) char buf[N * sizeof(std::string)];
    std::string* arr = reinterpret_cast<std::string*>(buf);
    for (int i = 0; i < N; ++i) {
        mystl::construct(arr + i, std::to_string(i));
    }
    mystl::destroy(arr, arr + N);  // 应逐个调用 ~string()
    // 如果内存没泄漏、没崩溃，测试通过
}

// ============================================================
// PoolAlloc — 基本分配 / 释放
// ============================================================

TEST(PoolAlloc, SmallAlloc) {
    void* p = mystl::PoolAlloc::allocate(8);
    ASSERT_NE(p, nullptr);
    // 写入一个值，读回，验证内存可用
    *static_cast<int*>(p) = 42;
    EXPECT_EQ(*static_cast<int*>(p), 42);
    mystl::PoolAlloc::deallocate(p, 8);
}

TEST(PoolAlloc, AllocMaxBytes) {
    // 128 字节是二级分配器的边界
    void* p = mystl::PoolAlloc::allocate(128);
    ASSERT_NE(p, nullptr);
    mystl::PoolAlloc::deallocate(p, 128);
}

TEST(PoolAlloc, LargeAllocFallsThrough) {
    // > 128 字节交给一级分配器
    void* p = mystl::PoolAlloc::allocate(256);
    ASSERT_NE(p, nullptr);
    mystl::PoolAlloc::deallocate(p, 256);
}

TEST(PoolAlloc, ReuseFreedBlock) {
    // 释放后再分配同档大小，应拿到同一块内存（freelist 复用）
    void* p1 = mystl::PoolAlloc::allocate(16);
    mystl::PoolAlloc::deallocate(p1, 16);
    void* p2 = mystl::PoolAlloc::allocate(16);
    EXPECT_EQ(p1, p2);  // 头部弹出，指针应相同
    mystl::PoolAlloc::deallocate(p2, 16);
}

TEST(PoolAlloc, MultipleAllocs) {
    // 连续分配 20 个 8 字节块，每块都应可用
    constexpr int N = 20;
    void* ptrs[N];
    for (int i = 0; i < N; ++i) {
        ptrs[i] = mystl::PoolAlloc::allocate(8);
        ASSERT_NE(ptrs[i], nullptr);
        *static_cast<int*>(ptrs[i]) = i;
    }
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(*static_cast<int*>(ptrs[i]), i);
        mystl::PoolAlloc::deallocate(ptrs[i], 8);
    }
}

// ============================================================
// Allocator<T> — 标准接口
// ============================================================

TEST(Allocator, AllocateInt) {
    mystl::Allocator<int> alloc;
    int* p = alloc.allocate(10);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 10; ++i) {
        alloc.construct(p + i, i * 2);
    }
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(p[i], i * 2);
        alloc.destroy(p + i);
    }
    alloc.deallocate(p, 10);
}

TEST(Allocator, Rebind) {
    mystl::Allocator<int> a_int;
    mystl::Allocator<int>::rebind<double>::other a_double;
    double* p = a_double.allocate(4);
    ASSERT_NE(p, nullptr);
    a_double.deallocate(p, 4);
    (void)a_int;
}

TEST(Allocator, WorksWithStdVector) {
    // 最终检验：能给 std::vector 用
    std::vector<int, mystl::Allocator<int>> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    for (int i = 0; i < 100; ++i) EXPECT_EQ(v[i], i);
}

// ============================================================
// uninitialized_copy / fill / fill_n
// ============================================================

TEST(Uninitialized, CopyTrivial) {
    int src[5] = {1, 2, 3, 4, 5};
    alignas(int) char buf[5 * sizeof(int)];
    int* dst = reinterpret_cast<int*>(buf);

    int* end = mystl::uninitialized_copy(src, src + 5, dst);
    EXPECT_EQ(end, dst + 5);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(dst[i], src[i]);
    mystl::destroy(dst, dst + 5);
}

TEST(Uninitialized, CopyNonTrivial) {
    std::string src[3] = {"a", "bb", "ccc"};
    alignas(std::string) char buf[3 * sizeof(std::string)];
    std::string* dst = reinterpret_cast<std::string*>(buf);

    mystl::uninitialized_copy(src, src + 3, dst);
    EXPECT_EQ(dst[0], "a");
    EXPECT_EQ(dst[1], "bb");
    EXPECT_EQ(dst[2], "ccc");
    mystl::destroy(dst, dst + 3);
}

TEST(Uninitialized, FillNonTrivial) {
    alignas(std::string) char buf[4 * sizeof(std::string)];
    std::string* arr = reinterpret_cast<std::string*>(buf);

    mystl::uninitialized_fill(arr, arr + 4, std::string("xyz"));
    for (int i = 0; i < 4; ++i) EXPECT_EQ(arr[i], "xyz");
    mystl::destroy(arr, arr + 4);
}

TEST(Uninitialized, FillN) {
    alignas(int) char buf[5 * sizeof(int)];
    int* arr = reinterpret_cast<int*>(buf);

    auto end = mystl::uninitialized_fill_n(arr, 5, 99);
    EXPECT_EQ(end, arr + 5);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(arr[i], 99);
    mystl::destroy(arr, arr + 5);
}
