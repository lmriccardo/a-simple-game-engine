#include <ASGE/Core/Concurrent/Thread.hpp>

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
        : Thread(inName, nullptr, inDaemon), limit(inLimit) {}

    // Must Cancel()+Join() here, not rely on ~Thread() -- see the
    // @warning on Thread's class doc comment.
    ~CountingThread() override { Cancel(); Join(); }

protected:
    void Run(context_pointer& ctx) override
    {
        ++count;
        if (count.load() >= limit) ctx->Cancel(ContextErr::Canceled);
    }

    void TearDown() override { tearDownCalled = true; }
};

class IdleThread final : public Thread
{
public:
    IdleThread() : Thread() {}
    explicit IdleThread(std::string const& inName) : Thread(inName) {}

    // Must Cancel()+Join() here, not rely on ~Thread() -- see the
    // @warning on Thread's class doc comment.
    ~IdleThread() override { Cancel(); Join(); }

protected:
    void Run(context_pointer&) override
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
using asge::concurrent::ContextErr;
using asge::concurrent::Background;
using asge::concurrent::context_pointer;

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

// Regression test for issue #40: Thread::~Thread() alone can't safely stop
// a background thread that dispatches virtual Run() calls on `this` --
// by the time a base-class destructor runs, the vtable has already
// reverted away from the derived override, so a worker thread still
// mid-loop could dispatch straight into Run()'s pure-virtual slot in
// Thread and crash ("pure virtual function called", reliably hit on
// macOS CI). The fix moves Cancel()+Join() into each subclass's own
// destructor instead. Repeating construct-start-destroy many times, with
// no explicit Cancel()/Join() by the caller, maximises the chance of
// landing in the race window that used to crash.
TEST(Thread, RepeatedStartAndImmediateDestructionDoesNotCrash)
{
    constexpr int kIterations = 300;
    for (int ii = 0; ii < kIterations; ++ii)
    {
        IdleThread t;
        t.Start();
    }
    for (int ii = 0; ii < kIterations; ++ii)
    {
        CountingThread t(1);
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

TEST(Thread, ThreadStaticStart_ReturnsNonNullPointer)
{
    auto callback = [](context_pointer& ctx) { ctx->Cancel(ContextErr::Canceled); };
    auto t = Thread::Start(false, nullptr, callback);
    ASSERT_TRUE(t);
    t->Join();
}

TEST(Thread, ThreadStaticStart_StartedFlagAndThreadIdSetAfterConstruction)
{
    auto callback = [](context_pointer& ctx) { ctx->Cancel(ContextErr::Canceled); };
    auto t = Thread::Start(false, nullptr, callback);
    EXPECT_TRUE(t->Started());
    EXPECT_NE(t->GetThreadId(), std::thread::id{});
    t->Join();
}

TEST(Thread, ThreadStaticStart_CallableIsInvoked)
{
    std::atomic<int> count{0};
    auto callback = [&count](context_pointer& ctx) {
        ++count;
        ctx->Cancel(ContextErr::Canceled);
    };
    auto t = Thread::Start(false, nullptr, callback);
    t->Join();
    EXPECT_GE(count.load(), 1);
}

TEST(Thread, ThreadStaticStart_CallableReceivesExtraArgs)
{
    std::atomic<int> result{0};
    auto callback = [&result](context_pointer& ctx, int x) {
        result.store(x);
        ctx->Cancel(ContextErr::Canceled);
    };
    auto t = Thread::Start(false, nullptr, callback, 42);
    t->Join();
    EXPECT_EQ(result.load(), 42);
}

TEST(Thread, ThreadStaticStart_DaemonThreadIsDetached)
{
    auto callback = [](context_pointer& ctx) { ctx->Cancel(ContextErr::Canceled); };
    auto t = Thread::Start(true, nullptr, callback);
    EXPECT_TRUE(t->Daemon());
    EXPECT_FALSE(t->Joinable());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST(Thread, ThreadStaticStart_CancelStopsCallableLoop)
{
    std::atomic<int> count{0};
    auto callback = [&count](context_pointer&) {
        ++count;
        std::this_thread::yield();
    };
    auto t = Thread::Start(false, nullptr, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    t->Cancel();
    t->Join();
    EXPECT_GT(count.load(), 0);
}

TEST(Thread, ThreadStaticStart_InheritsParentContext)
{
    auto parent = asge::concurrent::WithCancel(Background());
    std::atomic<bool> ran{false};

    auto callback = [&ran](context_pointer& ctx) {
        ran.store(true);
        ctx->Cancel(ContextErr::Canceled);
    };

    auto t = Thread::Start(false, parent, callback);
    t->Join();
    EXPECT_TRUE(ran.load());
}

TEST(Thread, ThreadStaticStart_ParentContextCancellationStopsThread)
{
    auto parent = asge::concurrent::WithCancel(Background());
    std::atomic<int> count{0};

    auto callback = [&count](context_pointer&) {
        ++count;
        std::this_thread::yield();
    };

    auto t = Thread::Start(false, parent, callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    parent->Cancel();
    t->Join();
    EXPECT_GT(count.load(), 0);
}

// Same regression as Thread.RepeatedStartAndImmediateDestructionDoesNotCrash
// (see issue #40), exercised through ThreadImpl -- the concrete class
// backing this static factory -- rather than a hand-written subclass. The
// returned thread_pointer is destroyed with no explicit Cancel()/Join() by
// the caller, relying on ThreadImpl's own destructor to do it safely.
TEST(Thread, ThreadStaticStart_RepeatedStartAndImmediateDestructionDoesNotCrash)
{
    constexpr int kIterations = 300;
    auto callback = [](context_pointer&) { std::this_thread::yield(); };
    for (int ii = 0; ii < kIterations; ++ii)
    {
        [[maybe_unused]] auto t = Thread::Start(false, nullptr, callback);
    }
    SUCCEED();
}

}
