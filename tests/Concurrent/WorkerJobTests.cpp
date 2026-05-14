#include <ASGE/Concurrent/ThreadPool.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace
{

using asge::concurrent::_internal::WorkerJob;

// ─── GetFuture ────────────────────────────────────────────────────────────────

TEST(WorkerJobTest, GetFuture_ReturnsValidFutureObject)
{
    WorkerJob job([](int x) { return x; }, 1);
    auto fut = job.GetFuture();
    EXPECT_TRUE(fut.valid());
}

TEST(WorkerJobTest, GetFuture_FutureNotReadyBeforeExecute)
{
    WorkerJob job([](int x) { return x; }, 1);
    auto fut = job.GetFuture();
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
}

// ─── Execute ──────────────────────────────────────────────────────────────────

TEST(WorkerJobTest, Execute_FutureBecomesReadyAfterExecute)
{
    WorkerJob job([](int x) { return x; }, 1);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
}

TEST(WorkerJobTest, Execute_ResultMatchesCallableReturnValue)
{
    WorkerJob job([](int x) { return x * 2; }, 21);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.get(), 42);
}

TEST(WorkerJobTest, Execute_MultipleStoredArgs)
{
    WorkerJob job([](int a, int b, int c) { return a + b + c; }, 1, 2, 3);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.get(), 6);
}

TEST(WorkerJobTest, Execute_DoubleReturnType)
{
    WorkerJob job([](double a, double b) { return a * b; }, 1.5, 4.0);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_DOUBLE_EQ(fut.get(), 6.0);
}

TEST(WorkerJobTest, Execute_StringReturnType)
{
    WorkerJob job([]() -> std::string { return "hello"; });
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.get(), "hello");
}

TEST(WorkerJobTest, Execute_LambdaWithCapture)
{
    int multiplier = 7;
    WorkerJob job([multiplier](int x) { return multiplier * x; }, 6);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.get(), 42);
}

TEST(WorkerJobTest, Execute_StdFunctionWrapper)
{
    std::function<int(int)> fn = [](int x) { return x + 1; };
    WorkerJob job(std::move(fn), 41);
    auto fut = job.GetFuture();
    job.Execute();
    EXPECT_EQ(fut.get(), 42);
}

// ─── Async execution ──────────────────────────────────────────────────────────

TEST(WorkerJobTest, Execute_CalledFromSeparateThread)
{
    WorkerJob job([](int a, int b) { return a + b; }, 10, 32);
    auto fut = job.GetFuture();
    std::thread t([&job]() { job.Execute(); });
    t.join();
    EXPECT_EQ(fut.get(), 42);
}

TEST(WorkerJobTest, Execute_FutureBlocksUntilExecuteCompletes)
{
    WorkerJob job([](int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return ms;
    }, 20);

    auto fut = job.GetFuture();
    std::thread t([&job]() { job.Execute(); });

    EXPECT_EQ(fut.get(), 20);
    t.join();
}

}
