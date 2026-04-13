#include <gtest/gtest.h>
#include "stack_queue.h"

// ==================== stack ====================
TEST(Stack, BasicOps) {
    mystl::stack<int> s;
    EXPECT_TRUE(s.empty());
    s.push(1); s.push(2); s.push(3);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.top(), 3);
    s.pop();
    EXPECT_EQ(s.top(), 2);
}

TEST(Stack, LIFO) {
    mystl::stack<int> s;
    for (int i = 1; i <= 5; ++i) s.push(i);
    for (int i = 5; i >= 1; --i) {
        EXPECT_EQ(s.top(), i);
        s.pop();
    }
    EXPECT_TRUE(s.empty());
}

// ==================== queue ====================
TEST(Queue, BasicOps) {
    mystl::queue<int> q;
    EXPECT_TRUE(q.empty());
    q.push(1); q.push(2); q.push(3);
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 3);
}

TEST(Queue, FIFO) {
    mystl::queue<int> q;
    for (int i = 1; i <= 5; ++i) q.push(i);
    for (int i = 1; i <= 5; ++i) {
        EXPECT_EQ(q.front(), i);
        q.pop();
    }
    EXPECT_TRUE(q.empty());
}
