#include <gtest/gtest.h>
#include "heap_priority_queue.h"
#include "vector.h"
#include <algorithm>

// 用 mystl::vector 避免与 std 的堆函数 ADL 歧义
using VI = mystl::vector<int>;

// ==================== heap 算法 ====================
TEST(Heap, MakeHeap) {
    VI v = {3, 1, 4, 1, 5, 9, 2, 6};
    mystl::make_heap(v.begin(), v.end());
    EXPECT_TRUE(mystl::is_heap(v.begin(), v.end()));
}

TEST(Heap, PushHeap) {
    VI v = {5, 3, 4};
    mystl::make_heap(v.begin(), v.end());
    v.push_back(8);
    mystl::push_heap(v.begin(), v.end());
    EXPECT_EQ(v[0], 8);
    EXPECT_TRUE(mystl::is_heap(v.begin(), v.end()));
}

TEST(Heap, PopHeap) {
    VI v = {9, 5, 7, 3, 2};
    mystl::make_heap(v.begin(), v.end());
    mystl::pop_heap(v.begin(), v.end());
    EXPECT_EQ(v.back(), 9);
    EXPECT_TRUE(mystl::is_heap(v.begin(), v.end() - 1));
}

TEST(Heap, SortHeap) {
    VI v = {5, 3, 8, 1, 7, 2, 4, 6};
    mystl::make_heap(v.begin(), v.end());
    mystl::sort_heap(v.begin(), v.end());
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(Heap, MinHeap_WithGreater) {
    VI v = {5, 3, 8, 1, 7};
    mystl::make_heap(v.begin(), v.end(), std::greater<int>{});
    EXPECT_EQ(v[0], 1);
    mystl::sort_heap(v.begin(), v.end(), std::greater<int>{});
    for (int i = 0; i < 4; ++i) EXPECT_GE(v[i], v[i+1]);
}

// ==================== priority_queue ====================
TEST(PriorityQueue, MaxHeap) {
    mystl::priority_queue<int> pq;
    pq.push(3); pq.push(1); pq.push(4); pq.push(1); pq.push(5); pq.push(9);
    EXPECT_EQ(pq.top(), 9);
    pq.pop();
    EXPECT_EQ(pq.top(), 5);
    EXPECT_EQ(pq.size(), 5u);
}

TEST(PriorityQueue, MinHeap) {
    mystl::priority_queue<int, mystl::vector<int>, std::greater<int>> pq;
    pq.push(5); pq.push(2); pq.push(8); pq.push(1);
    EXPECT_EQ(pq.top(), 1);
    pq.pop();
    EXPECT_EQ(pq.top(), 2);
}

TEST(PriorityQueue, FromRange) {
    VI v = {3, 1, 4, 1, 5, 9};
    mystl::priority_queue<int> pq(v.begin(), v.end());
    EXPECT_EQ(pq.top(), 9);
    EXPECT_EQ(pq.size(), 6u);
}

TEST(PriorityQueue, DrainOrder) {
    mystl::priority_queue<int> pq;
    for (int x : {3, 1, 4, 1, 5, 9, 2, 6}) pq.push(x);
    int prev = pq.top(); pq.pop();
    while (!pq.empty()) {
        EXPECT_GE(prev, pq.top());
        prev = pq.top(); pq.pop();
    }
}
