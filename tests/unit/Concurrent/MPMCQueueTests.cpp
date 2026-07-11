#include <ASGE/Core/Concurrent/Queues/MPMCQueue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace
{

using asge::concurrent::_internal::MPMCQueue;

// ─── Empty queue ──────────────────────────────────────────────────────────────

TEST(MPMCQueue, TryPopOnEmptyReturnsFalse)
{
    MPMCQueue<4, int> q;
    int out{};
    EXPECT_FALSE(q.TryPop(out));
}

TEST(MPMCQueue, TryPushOnEmptyReturnsTrue)
{
    MPMCQueue<4, int> q;
    EXPECT_TRUE(q.TryPush(1));
}

// ─── Single-element round-trip ────────────────────────────────────────────────

TEST(MPMCQueue, PushedValueIsPopped)
{
    MPMCQueue<4, int> q;
    q.TryPush(42);
    int out{};
    ASSERT_TRUE(q.TryPop(out));
    EXPECT_EQ(out, 42);
}

TEST(MPMCQueue, PopAfterSingleRoundTripLeavesQueueEmpty)
{
    MPMCQueue<4, int> q;
    q.TryPush(7);
    int out{};
    q.TryPop(out);
    EXPECT_FALSE(q.TryPop(out));
}

// ─── Capacity boundary ────────────────────────────────────────────────────────

TEST(MPMCQueue, FillsToCapacity)
{
    MPMCQueue<4, int> q;
    EXPECT_TRUE(q.TryPush(1));
    EXPECT_TRUE(q.TryPush(2));
    EXPECT_TRUE(q.TryPush(3));
    EXPECT_TRUE(q.TryPush(4));
}

TEST(MPMCQueue, TryPushOnFullReturnsFalse)
{
    MPMCQueue<4, int> q;
    for (int i = 0; i < 4; ++i) q.TryPush(i);
    EXPECT_FALSE(q.TryPush(99));
}

TEST(MPMCQueue, TryPushSucceedsAfterDraining)
{
    MPMCQueue<4, int> q;
    for (int i = 0; i < 4; ++i) q.TryPush(i);
    int out{};
    q.TryPop(out);
    EXPECT_TRUE(q.TryPush(99));
}

// ─── FIFO ordering ────────────────────────────────────────────────────────────

TEST(MPMCQueue, MaintainsFIFOOrder)
{
    MPMCQueue<8, int> q;
    for (int i = 0; i < 5; ++i) q.TryPush(i);
    for (int i = 0; i < 5; ++i)
    {
        int out{};
        ASSERT_TRUE(q.TryPop(out));
        EXPECT_EQ(out, i);
    }
}

// ─── Wrap-around ──────────────────────────────────────────────────────────────

TEST(MPMCQueue, CorrectAfterMultipleFillDrainCycles)
{
    MPMCQueue<4, int> q;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(q.TryPush(i + cycle * 10));
        EXPECT_FALSE(q.TryPush(-1));
        for (int i = 0; i < 4; ++i)
        {
            int out{};
            ASSERT_TRUE(q.TryPop(out));
            EXPECT_EQ(out, i + cycle * 10);
        }
        int out{};
        EXPECT_FALSE(q.TryPop(out));
    }
}

// ─── Move semantics ───────────────────────────────────────────────────────────

TEST(MPMCQueue, MoveOnlyTypeCanBePushedAndPopped)
{
    MPMCQueue<4, std::unique_ptr<int>> q;
    ASSERT_TRUE(q.TryPush(std::make_unique<int>(123)));
    std::unique_ptr<int> out;
    ASSERT_TRUE(q.TryPop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, 123);
}

// ─── Concurrent: multiple producers ──────────────────────────────────────────

TEST(MPMCQueue, MultipleProducersDeliverAllItems)
{
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 64;
    MPMCQueue<512, int> q;

    std::atomic<int> pushed{0};
    std::vector<std::thread> producers;
    producers.reserve(kProducers);

    for (int p = 0; p < kProducers; ++p)
    {
        producers.emplace_back([&q, &pushed, p]() {
            for (int i = 0; i < kPerProducer; ++i)
            {
                while (!q.TryPush(p * kPerProducer + i))
                    std::this_thread::yield();
                ++pushed;
            }
        });
    }

    for (auto& t : producers) t.join();
    EXPECT_EQ(pushed.load(), kProducers * kPerProducer);
}

// ─── Concurrent: multiple consumers ──────────────────────────────────────────

TEST(MPMCQueue, MultipleConsumersDrainAllItems)
{
    constexpr int kItems = 256;
    constexpr int kConsumers = 4;
    MPMCQueue<512, int> q;

    for (int i = 0; i < kItems; ++i) q.TryPush(i);

    std::atomic<int> popped{0};
    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);

    for (int c = 0; c < kConsumers; ++c)
    {
        consumers.emplace_back([&q, &popped]() {
            int out{};
            while (popped.load() < kItems)
            {
                if (q.TryPop(out)) ++popped;
                else std::this_thread::yield();
            }
        });
    }

    for (auto& t : consumers) t.join();
    EXPECT_EQ(popped.load(), kItems);
}

// ─── Concurrent: producers and consumers interleaved ─────────────────────────

TEST(MPMCQueue, ConcurrentProducersAndConsumersPreserveCount)
{
    constexpr int kProducers  = 4;
    constexpr int kConsumers  = 4;
    constexpr int kPerProducer = 512;
    constexpr int kTotal = kProducers * kPerProducer;

    MPMCQueue<256, int> q;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> workers;
    workers.reserve(kProducers + kConsumers);

    for (int p = 0; p < kProducers; ++p)
    {
        workers.emplace_back([&q, &produced]() {
            for (int i = 0; i < kPerProducer; ++i)
            {
                while (!q.TryPush(i)) std::this_thread::yield();
                ++produced;
            }
        });
    }

    for (int c = 0; c < kConsumers; ++c)
    {
        workers.emplace_back([&q, &consumed]() {
            int out{};
            while (consumed.load() < kTotal)
            {
                if (q.TryPop(out)) ++consumed;
                else std::this_thread::yield();
            }
        });
    }

    for (auto& t : workers) t.join();

    EXPECT_EQ(produced.load(), kTotal);
    EXPECT_EQ(consumed.load(), kTotal);
}

}
