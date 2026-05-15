#include <ASGE/Core/Concurrent/ThreadPool.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace
{

using asge::concurrent::ThreadPool;

// Spins until predicate is true or the timeout elapses. Returns the final value.
template<typename Pred>
static bool WaitFor(Pred pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    return pred();
}

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(ThreadPoolTest, Construction_DefaultWorkerCount)
{
    EXPECT_NO_THROW({ ThreadPool pool; });
}

TEST(ThreadPoolTest, Construction_ExplicitWorkerCount)
{
    EXPECT_NO_THROW({ ThreadPool pool(4); });
}

TEST(ThreadPoolTest, Construction_SingleWorker)
{
    EXPECT_NO_THROW({ ThreadPool pool(1); });
}

// ─── GetPoolContext ───────────────────────────────────────────────────────────

TEST(ThreadPoolTest, GetPoolContext_ReturnsNonNull)
{
    ThreadPool pool(1);
    EXPECT_NE(pool.GetPoolContext(), nullptr);
}

// ─── Submit: void callable ────────────────────────────────────────────────────

TEST(ThreadPoolTest, Submit_VoidCallableIsEventuallyExecuted)
{
    ThreadPool pool(2);
    std::atomic<bool> executed{false};
    pool.Submit([&executed]() { executed.store(true); });
    EXPECT_TRUE(WaitFor([&]{ return executed.load(); }));
}

TEST(ThreadPoolTest, Submit_VoidCallableWithStoredArg)
{
    ThreadPool pool(2);
    std::atomic<int> result{0};
    pool.Submit([&result](int x) { result.store(x); }, 42);
    EXPECT_TRUE(WaitFor([&]{ return result.load() == 42; }));
}

TEST(ThreadPoolTest, Submit_MultipleVoidJobsAllExecuted)
{
    constexpr int kJobs = 16;
    ThreadPool pool(4);
    std::atomic<int> count{0};
    for (int i = 0; i < kJobs; ++i)
        pool.Submit([&count]() { count.fetch_add(1); });
    EXPECT_TRUE(WaitFor([&]{ return count.load() == kJobs; }));
}

TEST(ThreadPoolTest, Submit_VoidCallableWithMultipleStoredArgs)
{
    ThreadPool pool(2);
    std::atomic<int> result{0};
    pool.Submit([&result](int a, int b) { result.store(a + b); }, 10, 32);
    EXPECT_TRUE(WaitFor([&]{ return result.load() == 42; }));
}

// ─── Submit: non-void callable (returns future) ───────────────────────────────

TEST(ThreadPoolTest, Submit_NonVoidCallableReturnsFuture)
{
    ThreadPool pool(2);
    auto fut = pool.Submit([](int x) { return x * 2; }, 21);
    EXPECT_EQ(fut.get(), 42);
}

TEST(ThreadPoolTest, Submit_FutureResolvesSumOfStoredArgs)
{
    ThreadPool pool(2);
    auto fut = pool.Submit([](int a, int b) { return a + b; }, 10, 32);
    EXPECT_EQ(fut.get(), 42);
}

TEST(ThreadPoolTest, Submit_FutureResolvesDoubleReturnType)
{
    ThreadPool pool(2);
    auto fut = pool.Submit([](double x) { return x * x; }, 3.0);
    EXPECT_DOUBLE_EQ(fut.get(), 9.0);
}

TEST(ThreadPoolTest, Submit_FutureResolvesStringReturnType)
{
    ThreadPool pool(2);
    auto fut = pool.Submit([]() -> std::string { return "hello"; });
    EXPECT_EQ(fut.get(), "hello");
}

TEST(ThreadPoolTest, Submit_MultipleNonVoidJobsAllResolve)
{
    constexpr int kJobs = 8;
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    futures.reserve(kJobs);
    for (int i = 0; i < kJobs; ++i)
        futures.push_back(pool.Submit([](int x) { return x; }, i));
    for (int i = 0; i < kJobs; ++i)
        EXPECT_EQ(futures[i].get(), i);
}

TEST(ThreadPoolTest, Submit_FutureFromLambdaWithCapture)
{
    ThreadPool pool(2);
    int multiplier = 6;
    auto fut = pool.Submit([multiplier](int x) { return multiplier * x; }, 7);
    EXPECT_EQ(fut.get(), 42);
}

// ─── Destructor ───────────────────────────────────────────────────────────────

TEST(ThreadPoolTest, Destructor_JoinsAllWorkersCleanly)
{
    {
        ThreadPool pool(4);
        pool.Submit([]() {});
    }
    SUCCEED();
}

TEST(ThreadPoolTest, Destructor_WithActiveJobsDoesNotHang)
{
    {
        ThreadPool pool(2);
        for (int i = 0; i < 8; ++i)
            pool.Submit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
    }
    SUCCEED();
}

// ─── Concurrency ─────────────────────────────────────────────────────────────

TEST(ThreadPoolTest, Concurrency_ConcurrentSubmitsFromMultipleThreads)
{
    constexpr int kThreads   = 4;
    constexpr int kPerThread = 16;
    ThreadPool pool(4);
    std::atomic<int> count{0};

    std::vector<std::thread> submitters;
    submitters.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        submitters.emplace_back([&pool, &count]() {
            for (int i = 0; i < kPerThread; ++i)
                pool.Submit([&count]() { count.fetch_add(1); });
        });
    }
    for (auto& t : submitters) t.join();

    EXPECT_TRUE(WaitFor([&]{ return count.load() == kThreads * kPerThread; }));
}

TEST(ThreadPoolTest, Concurrency_MultipleFuturesResolveCorrectly)
{
    constexpr int kJobs = 32;
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    futures.reserve(kJobs);

    for (int i = 0; i < kJobs; ++i)
        futures.push_back(pool.Submit([](int x) { return x * x; }, i));

    for (int i = 0; i < kJobs; ++i)
        EXPECT_EQ(futures[i].get(), i * i);
}

}