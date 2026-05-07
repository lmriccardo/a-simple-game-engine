#include <ASGE/Concurrent/Thread.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace asge::concurrent
{

class CountingThread final : public Thread
{
public:
    std::atomic<int> count{0};
    int              limit;
    bool             tearDownCalled{false};

    explicit CountingThread(int inLimit)
        : Thread(), limit(inLimit) {}

    CountingThread(std::string const& inName, int inLimit, bool inDaemon = false)
        : Thread(inName, inDaemon), limit(inLimit) {}

protected:
    void Run(ThreadContext& ctx) override
    {
        ++count;
        if (count.load() >= limit) ctx.Cancel();
    }

    void TearDown() override { tearDownCalled = true; }
};

class IdleThread final : public Thread
{
public:
    IdleThread() : Thread() {}
    explicit IdleThread(std::string const& inName) : Thread(inName) {}

protected:
    void Run(ThreadContext&) override
    {
        std::this_thread::yield();
    }
};

}

namespace
{

using asge::concurrent::CountingThread;
using asge::concurrent::IdleThread;
using asge::concurrent::Thread;
using asge::concurrent::ThreadContext;

// ─── ThreadContext ────────────────────────────────────────────────────────────

TEST(ThreadContext, DefaultsToNotCancelled)
{
    ThreadContext ctx;
    EXPECT_FALSE(ctx.Cancelled());
}

TEST(ThreadContext, CancelSetsCancelledTrue)
{
    ThreadContext ctx;
    ctx.Cancel();
    EXPECT_TRUE(ctx.Cancelled());
}

TEST(ThreadContext, CancelIsIdempotent)
{
    ThreadContext ctx;
    ctx.Cancel();
    ctx.Cancel();
    EXPECT_TRUE(ctx.Cancelled());
}

// ─── Thread ───────────────────────────────────────────────────────────────────

TEST(Thread, DefaultNameMatchesPattern)
{
    CountingThread t(1);
    EXPECT_EQ(t.GetName().rfind("ASGE_Thread_", 0), 0U);
}

TEST(Thread, CustomNameIsPreserved)
{
    CountingThread t("WorkerThread", 1);
    EXPECT_EQ(t.GetName(), "WorkerThread");
}

TEST(Thread, DaemonFlagDefaultsToFalse)
{
    CountingThread t(1);
    EXPECT_FALSE(t.Daemon());
}

TEST(Thread, DaemonFlagSetViaConstructor)
{
    CountingThread t("DaemonThread", 1, /*daemon=*/true);
    EXPECT_TRUE(t.Daemon());
}

TEST(Thread, NotStartedInitially)
{
    CountingThread t(1);
    EXPECT_FALSE(t.Started());
}

TEST(Thread, NotJoinableBeforeStart)
{
    CountingThread t(1);
    EXPECT_FALSE(t.Joinable());
}

TEST(Thread, StartSetsStartedFlag)
{
    IdleThread t;
    t.Start();
    EXPECT_TRUE(t.Started());
    t.Cancel();
    t.Join();
}

TEST(Thread, JoinableAfterStartForNonDaemon)
{
    IdleThread t;
    t.Start();
    EXPECT_TRUE(t.Joinable());
    t.Cancel();
    t.Join();
}

TEST(Thread, ThreadIdValidAfterStart)
{
    IdleThread t;
    EXPECT_EQ(t.GetThreadId(), std::thread::id{});
    t.Start();
    EXPECT_NE(t.GetThreadId(), std::thread::id{});
    t.Cancel();
    t.Join();
}

TEST(Thread, RunExecutesWorkload)
{
    CountingThread t(5);
    t.Start();
    t.Join();
    EXPECT_GE(t.count.load(), 5);
}

TEST(Thread, CancelStopsThreadLoop)
{
    IdleThread t;
    t.Start();
    t.Cancel();
    t.Join();
    EXPECT_FALSE(t.Joinable());
}

TEST(Thread, StartIsIdempotent)
{
    IdleThread t;
    t.Start();
    t.Start();
    EXPECT_TRUE(t.Started());
    t.Cancel();
    t.Join();
}

TEST(Thread, JoinOnNotStartedIsNoop)
{
    CountingThread t(1);
    EXPECT_NO_THROW(t.Join());
}

TEST(Thread, TearDownCalledAfterJoin)
{
    CountingThread t(1);
    t.Start();
    t.Join();
    EXPECT_TRUE(t.tearDownCalled);
}

TEST(Thread, DestructorCancelsAndJoins)
{
    {
        IdleThread t;
        t.Start();
    }
    SUCCEED();
}

TEST(Thread, DetachMakesDaemon)
{
    IdleThread t;
    t.Start();
    t.Detach();
    EXPECT_TRUE(t.Daemon());
    EXPECT_FALSE(t.Joinable());
    t.Cancel();
    // allow the detached thread to observe the cancellation before t is destroyed
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// ─── Thread::Start (static factory) ──────────────────────────────────────────

TEST(ThreadStaticStart, ReturnsNonNullPointer)
{
    auto callback = [](ThreadContext& ctx) { ctx.Cancel(); };
    auto t = Thread::Start(false, callback);
    ASSERT_TRUE(t);
    t->Join();
}

TEST(ThreadStaticStart, StartedFlagAndThreadIdSetAfterConstruction)
{
    auto callback = [](ThreadContext& ctx) { ctx.Cancel(); };
    auto t = Thread::Start(false, callback);
    EXPECT_TRUE(t->Started());
    EXPECT_NE(t->GetThreadId(), std::thread::id{});
    t->Join();
}

TEST(ThreadStaticStart, CallableIsInvoked)
{
    std::atomic<int> count{0};
    auto callback = [&count](ThreadContext& ctx) {
        ++count;
        ctx.Cancel();
    };
    auto t = Thread::Start(false, callback);
    t->Join();
    EXPECT_GE(count.load(), 1);
}

TEST(ThreadStaticStart, CallableReceivesExtraArgs)
{
    std::atomic<int> result{0};
    auto callback = [&result](ThreadContext& ctx, int x) {
        result.store(x);
        ctx.Cancel();
    };
    auto t = Thread::Start(false, callback, 42);
    t->Join();
    EXPECT_EQ(result.load(), 42);
}

TEST(ThreadStaticStart, DaemonThreadIsDetached)
{
    auto callback = [](ThreadContext& ctx) { ctx.Cancel(); };
    auto t = Thread::Start(true, callback);
    EXPECT_TRUE(t->Daemon());
    EXPECT_FALSE(t->Joinable());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST(ThreadStaticStart, CancelStopsCallableLoop)
{
    std::atomic<int> count{0};
    auto callback = [&count](ThreadContext&) {
        ++count;
        std::this_thread::yield();
    };
    auto t = Thread::Start(false, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    t->Cancel();
    t->Join();
    EXPECT_GT(count.load(), 0);
}

}
