#include <gtest/gtest.h>
#include "deque.h"

using mystl::deque;

TEST(Deque, DefaultEmpty) {
    deque<int> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
}

TEST(Deque, PushBack) {
    deque<int> d;
    for (int i = 0; i < 100; ++i) d.push_back(i);
    EXPECT_EQ(d.size(), 100u);
    for (int i = 0; i < 100; ++i) EXPECT_EQ(d[i], i);
}

TEST(Deque, PushFront) {
    deque<int> d;
    for (int i = 0; i < 50; ++i) d.push_front(i);
    EXPECT_EQ(d.size(), 50u);
    EXPECT_EQ(d.front(), 49);
    EXPECT_EQ(d.back(), 0);
}

TEST(Deque, MixedPushPop) {
    deque<int> d;
    d.push_back(3);
    d.push_front(2);
    d.push_front(1);
    d.push_back(4);
    EXPECT_EQ(d.front(), 1);
    EXPECT_EQ(d.back(), 4);
    EXPECT_EQ(d.size(), 4u);

    d.pop_front();
    EXPECT_EQ(d.front(), 2);
    d.pop_back();
    EXPECT_EQ(d.back(), 3);
}

TEST(Deque, RandomAccess) {
    deque<int> d;
    for (int i = 0; i < 20; ++i) d.push_back(i * 10);
    for (int i = 0; i < 20; ++i) EXPECT_EQ(d[i], i * 10);
    EXPECT_EQ(d.at(5), 50);
    EXPECT_THROW(d.at(100), std::out_of_range);
}

TEST(Deque, Iterate) {
    deque<int> d = {1, 2, 3, 4, 5};
    int sum = 0;
    for (auto v : d) sum += v;
    EXPECT_EQ(sum, 15);
}

TEST(Deque, ReverseIterate) {
    deque<int> d = {1, 2, 3};
    std::vector<int> rev;
    for (auto it = d.rbegin(); it != d.rend(); ++it) rev.push_back(*it);
    EXPECT_EQ(rev[0], 3);
    EXPECT_EQ(rev[2], 1);
}

TEST(Deque, Clear) {
    deque<int> d = {1, 2, 3};
    d.clear();
    EXPECT_TRUE(d.empty());
}

TEST(Deque, CopyConstruct) {
    deque<int> a;
    for (int i = 0; i < 10; ++i) a.push_back(i);
    deque<int> b = a;
    for (int i = 0; i < 10; ++i) EXPECT_EQ(b[i], i);
}
