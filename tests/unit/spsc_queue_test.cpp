// tests/unit/spsc_queue_test.cpp
//
// T030 — Unit tests for SpscCommandQueue (T012).
//
// Tests verify lock-free push/pop behavior, capacity management, and
// thread-safety constraints. No allocation should occur on the hot path.

#include <gtest/gtest.h>
#include <concurrency/spsc_queue.h>

#include <thread>
#include <vector>
#include <chrono>

using namespace jyglobalvst::shared;

struct TestCommand
{
    int type {0};
    int value {0};

    TestCommand() = default;
    TestCommand(int t, int v) : type(t), value(v) {}
};

class SpscQueueTest : public ::testing::Test
{
protected:
    using Queue = SpscCommandQueue<TestCommand, 16>;
};

TEST_F(SpscQueueTest, PushAndPopSingleElement)
{
    Queue q;
    TestCommand cmd(1, 42);
    ASSERT_TRUE(q.tryPush(cmd));

    TestCommand out;
    ASSERT_TRUE(q.tryPop(out));
    EXPECT_EQ(out.type, 1);
    EXPECT_EQ(out.value, 42);
}

TEST_F(SpscQueueTest, PopEmptyQueueFails)
{
    Queue q;
    TestCommand out;
    ASSERT_FALSE(q.tryPop(out));
}

TEST_F(SpscQueueTest, PushToCapacity)
{
    Queue q;
    const auto cap = q.capacity();

    for (int i = 0; i < static_cast<int>(cap); ++i)
    {
        ASSERT_TRUE(q.tryPush(TestCommand(i, i * 10)));
    }

    // Next push should fail (full).
    ASSERT_FALSE(q.tryPush(TestCommand(-1, -1)));
}

TEST_F(SpscQueueTest, PushAfterPopFreesSlotsForReuse)
{
    Queue q;

    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(q.tryPush(TestCommand(i, i)));
    }

    TestCommand out;
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(q.tryPop(out));
        EXPECT_EQ(out.type, i);
    }

    // Should be able to push again after draining.
    for (int i = 3; i < 6; ++i)
    {
        ASSERT_TRUE(q.tryPush(TestCommand(i, i)));
    }
}

TEST_F(SpscQueueTest, ApproxSizeTracking)
{
    Queue q;
    EXPECT_EQ(q.approxSize(), 0);

    ASSERT_TRUE(q.tryPush(TestCommand(1, 1)));
    EXPECT_EQ(q.approxSize(), 1);

    ASSERT_TRUE(q.tryPush(TestCommand(2, 2)));
    EXPECT_EQ(q.approxSize(), 2);

    TestCommand out;
    ASSERT_TRUE(q.tryPop(out));
    EXPECT_EQ(q.approxSize(), 1);
}

TEST_F(SpscQueueTest, IdempotentPop)
{
    Queue q;
    TestCommand out;

    // Popping from empty multiple times should all fail.
    ASSERT_FALSE(q.tryPop(out));
    ASSERT_FALSE(q.tryPop(out));
    ASSERT_FALSE(q.tryPop(out));
}

TEST_F(SpscQueueTest, WraparoundCapacity)
{
    Queue q;
    const auto cap = q.capacity();

    // Fill, drain, refill to test index wraparound.
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        for (int i = 0; i < static_cast<int>(cap); ++i)
        {
            ASSERT_TRUE(q.tryPush(TestCommand(cycle, i)));
        }

        TestCommand out;
        for (int i = 0; i < static_cast<int>(cap); ++i)
        {
            ASSERT_TRUE(q.tryPop(out));
            EXPECT_EQ(out.type, cycle);
            EXPECT_EQ(out.value, i);
        }

        EXPECT_EQ(q.approxSize(), 0);
    }
}

// Stress test: concurrent push from one thread, pop from another.
TEST_F(SpscQueueTest, ConcurrentProducerConsumer)
{
    Queue q;
    const int num_items = 1000;
    int consumed_count = 0;

    std::thread producer([&q, num_items]() {
        for (int i = 0; i < num_items; ++i)
        {
            while (!q.tryPush(TestCommand(i % 10, i)))
            {
                // Spin if full; in production, exponential backoff or
                // MessageManager::callAsync would be used.
            }
        }
    });

    std::thread consumer([&q, &consumed_count, num_items]() {
        TestCommand out;
        while (consumed_count < num_items)
        {
            if (q.tryPop(out))
            {
                consumed_count++;
            }
            else
            {
                // Tiny sleep to avoid busy-waiting (not needed in production audio path).
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed_count, num_items);
    EXPECT_EQ(q.approxSize(), 0);
}
