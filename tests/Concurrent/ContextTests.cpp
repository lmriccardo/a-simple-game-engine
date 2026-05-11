#include <ASGE/Concurrent/Context.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace
{

using asge::concurrent::Context;
using asge::concurrent::ContextErr;
using asge::concurrent::ContextWithValue;
using asge::concurrent::Background;
using asge::concurrent::WithCancel;
using asge::concurrent::WithValue;
using asge::concurrent::CtxErrStr;
using asge::concurrent::context_pointer;

// ─── CtxErrStr ────────────────────────────────────────────────────────────────

TEST(ContextTest, CtxErrStr_NilReturnsNIL)
{
    EXPECT_EQ(CtxErrStr(ContextErr::Nil), "NIL");
}

TEST(ContextTest, CtxErrStr_CanceledReturnsCanceled)
{
    EXPECT_EQ(CtxErrStr(ContextErr::Canceled), "CANCELED");
}

TEST(ContextTest, CtxErrStr_DeadlineReturnsDeadlineExceeded)
{
    EXPECT_EQ(CtxErrStr(ContextErr::Deadline), "Deadline Exceeded");
}

// ─── Background ───────────────────────────────────────────────────────────────

TEST(ContextTest, Background_ReturnsNonNull)
{
    EXPECT_NE(Background(), nullptr);
}

TEST(ContextTest, Background_ReturnsSameInstance)
{
    EXPECT_EQ(Background(), Background());
}

TEST(ContextTest, Background_NeverDone)
{
    EXPECT_FALSE(Background()->Done());
}

TEST(ContextTest, Background_ErrIsNil)
{
    EXPECT_EQ(Background()->Err(), ContextErr::Nil);
}

TEST(ContextTest, Background_WaitReturnsImmediately)
{
    EXPECT_NO_THROW(Background()->Wait());
}

// ─── WithCancel — creation ────────────────────────────────────────────────────

TEST(ContextTest, WithCancel_ReturnsNonNull)
{
    auto ctx = WithCancel(Background());
    EXPECT_NE(ctx, nullptr);
}

TEST(ContextTest, WithCancel_NullParentUsesBackground)
{
    auto ctx = WithCancel(nullptr);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->Done());
    EXPECT_EQ(ctx->Err(), ContextErr::Nil);
}

TEST(ContextTest, WithCancel_NotDoneInitially)
{
    auto ctx = WithCancel(Background());
    EXPECT_FALSE(ctx->Done());
}

TEST(ContextTest, WithCancel_ErrIsNilInitially)
{
    auto ctx = WithCancel(Background());
    EXPECT_EQ(ctx->Err(), ContextErr::Nil);
}

TEST(ContextTest, WithCancel_OnAlreadyCanceledParentIsImmediatelyCanceled)
{
    auto parent = WithCancel(Background());
    parent->Cancel();
    auto child = WithCancel(parent);
    EXPECT_TRUE(child->Done());
    EXPECT_NE(child->Err(), ContextErr::Nil);
}

// ─── Context::Cancel ──────────────────────────────────────────────────────────

TEST(ContextTest, Cancel_SetsDoneToTrue)
{
    auto ctx = WithCancel(Background());
    ctx->Cancel();
    EXPECT_TRUE(ctx->Done());
}

TEST(ContextTest, Cancel_SetsErrToCanceled)
{
    auto ctx = WithCancel(Background());
    ctx->Cancel();
    EXPECT_EQ(ctx->Err(), ContextErr::Canceled);
}

TEST(ContextTest, Cancel_IsIdempotent)
{
    auto ctx = WithCancel(Background());
    ctx->Cancel();
    ctx->Cancel();
    EXPECT_TRUE(ctx->Done());
    EXPECT_EQ(ctx->Err(), ContextErr::Canceled);
}

// ─── Context::Wait ────────────────────────────────────────────────────────────

TEST(ContextTest, Wait_ReturnsImmediatelyIfAlreadyCanceled)
{
    auto ctx = WithCancel(Background());
    ctx->Cancel();
    EXPECT_NO_THROW(ctx->Wait());
}

TEST(ContextTest, Wait_BlocksUntilCanceledFromAnotherThread)
{
    auto ctx = WithCancel(Background());

    std::thread canceler([&ctx]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ctx->Cancel();
    });

    ctx->Wait();
    EXPECT_TRUE(ctx->Done());
    canceler.join();
}

// ─── Parent → child propagation ───────────────────────────────────────────────

TEST(ContextTest, Propagation_CancelingParentCancelsChild)
{
    auto parent = WithCancel(Background());
    auto child  = WithCancel(parent);

    parent->Cancel();

    EXPECT_TRUE(child->Done());
    EXPECT_EQ(child->Err(), ContextErr::Canceled);
}

TEST(ContextTest, Propagation_CancelingChildDoesNotCancelParent)
{
    auto parent = WithCancel(Background());
    auto child  = WithCancel(parent);

    child->Cancel();

    EXPECT_FALSE(parent->Done());
    EXPECT_EQ(parent->Err(), ContextErr::Nil);
}

TEST(ContextTest, Propagation_ParentCancelsAllChildren)
{
    auto parent = WithCancel(Background());
    auto c1     = WithCancel(parent);
    auto c2     = WithCancel(parent);
    auto c3     = WithCancel(parent);

    parent->Cancel();

    EXPECT_TRUE(c1->Done());
    EXPECT_TRUE(c2->Done());
    EXPECT_TRUE(c3->Done());
}

TEST(ContextTest, Propagation_CancellationCascadesToGrandchildren)
{
    auto root   = WithCancel(Background());
    auto parent = WithCancel(root);
    auto child  = WithCancel(parent);

    root->Cancel();

    EXPECT_TRUE(parent->Done());
    EXPECT_TRUE(child->Done());
    EXPECT_EQ(parent->Err(), ContextErr::Canceled);
    EXPECT_EQ(child->Err(), ContextErr::Canceled);
}

TEST(ContextTest, Propagation_ErrOnChildMatchesParentCancellationReason)
{
    auto parent = WithCancel(Background());
    auto child  = WithCancel(parent);

    parent->Cancel();

    EXPECT_EQ(child->Err(), ContextErr::Canceled);
}

TEST(ContextTest, Propagation_SiblingCancellationIsIsolated)
{
    auto parent   = WithCancel(Background());
    auto sibling1 = WithCancel(parent);
    auto sibling2 = WithCancel(parent);

    sibling1->Cancel();

    EXPECT_TRUE(sibling1->Done());
    EXPECT_FALSE(sibling2->Done());
    EXPECT_FALSE(parent->Done());
}

// ─── Context::Done via parent chain ───────────────────────────────────────────

TEST(ContextTest, Done_ReflectsParentCancellation)
{
    auto parent = WithCancel(Background());
    auto child  = WithCancel(parent);

    parent->Cancel();

    EXPECT_TRUE(child->Done());
}

TEST(ContextTest, Done_IndependentContextsDoNotAffectEachOther)
{
    auto ctx1 = WithCancel(Background());
    auto ctx2 = WithCancel(Background());

    ctx1->Cancel();

    EXPECT_TRUE(ctx1->Done());
    EXPECT_FALSE(ctx2->Done());
}

// ─── Concurrent cancellation ──────────────────────────────────────────────────

TEST(ContextTest, Concurrent_WaitUnblocksAllWaitersOnCancel)
{
    auto ctx = WithCancel(Background());
    constexpr int kWaiters = 4;

    std::atomic<int> unblocked{0};
    std::vector<std::thread> waiters;
    waiters.reserve(kWaiters);

    for (int i = 0; i < kWaiters; ++i)
    {
        waiters.emplace_back([&ctx, &unblocked]() {
            ctx->Wait();
            ++unblocked;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ctx->Cancel();

    for (auto& t : waiters) t.join();
    EXPECT_EQ(unblocked.load(), kWaiters);
}

// ─── ContextWithValue — creation ──────────────────────────────────────────────

TEST(ContextTest, WithValue_ReturnsNonNull)
{
    auto ctx = WithValue(42, Background());
    EXPECT_NE(ctx, nullptr);
}

TEST(ContextTest, WithValue_NullParentUsesBackground)
{
    auto ctx = WithValue(42, nullptr);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->Done());
    EXPECT_EQ(ctx->Err(), ContextErr::Nil);
}

TEST(ContextTest, WithValue_NotDoneInitially)
{
    auto ctx = WithValue(42, Background());
    EXPECT_FALSE(ctx->Done());
}

TEST(ContextTest, WithValue_ErrIsNilInitially)
{
    auto ctx = WithValue(42, Background());
    EXPECT_EQ(ctx->Err(), ContextErr::Nil);
}

// ─── ContextWithValue — Value() ───────────────────────────────────────────────

TEST(ContextTest, WithValue_IntValueIsPreserved)
{
    auto ctx = WithValue(99, Background());
    EXPECT_EQ(ctx->Value(), 99);
}

TEST(ContextTest, WithValue_StringValueIsPreserved)
{
    auto ctx = WithValue(std::string("hello"), Background());
    EXPECT_EQ(ctx->Value(), "hello");
}

TEST(ContextTest, WithValue_StructValueIsPreserved)
{
    struct Point { int x, y; };
    auto ctx = WithValue(Point{3, 7}, Background());
    EXPECT_EQ(ctx->Value().x, 3);
    EXPECT_EQ(ctx->Value().y, 7);
}

TEST(ContextTest, WithValue_ValueIsPreservedAfterCancel)
{
    auto ctx = WithValue(42, Background());
    ctx->Cancel();
    EXPECT_EQ(ctx->Value(), 42);
}

// ─── ContextWithValue — cancel behavior ──────────────────────────────────────

TEST(ContextTest, WithValue_CancelSetsDoneToTrue)
{
    auto ctx = WithValue(1, Background());
    ctx->Cancel();
    EXPECT_TRUE(ctx->Done());
}

TEST(ContextTest, WithValue_CancelSetsErrToCanceled)
{
    auto ctx = WithValue(1, Background());
    ctx->Cancel();
    EXPECT_EQ(ctx->Err(), ContextErr::Canceled);
}

TEST(ContextTest, WithValue_OnAlreadyCanceledParentIsImmediatelyCanceled)
{
    auto parent = WithCancel(Background());
    parent->Cancel();
    auto ctx = WithValue(42, parent);
    EXPECT_TRUE(ctx->Done());
    EXPECT_NE(ctx->Err(), ContextErr::Nil);
}

// ─── ContextWithValue — propagation ──────────────────────────────────────────

TEST(ContextTest, WithValue_ParentCancelPropagatesToValueContext)
{
    auto parent = WithCancel(Background());
    auto ctx    = WithValue(10, parent);

    parent->Cancel();

    EXPECT_TRUE(ctx->Done());
    EXPECT_EQ(ctx->Err(), ContextErr::Canceled);
}

TEST(ContextTest, WithValue_CancelPropagatesToCancelChildren)
{
    auto ctx   = WithValue(10, Background());
    auto child = WithCancel(ctx);

    ctx->Cancel();

    EXPECT_TRUE(child->Done());
    EXPECT_EQ(child->Err(), ContextErr::Canceled);
}

TEST(ContextTest, WithValue_CancelPropagatesToValueChildren)
{
    auto parent = WithValue(1, Background());
    auto child  = WithValue(2, parent);

    parent->Cancel();

    EXPECT_TRUE(child->Done());
    EXPECT_EQ(child->Err(), ContextErr::Canceled);
    EXPECT_EQ(child->Value(), 2);
}

TEST(ContextTest, WithValue_CancelingChildDoesNotCancelParent)
{
    auto parent = WithValue(1, Background());
    auto child  = WithValue(2, parent);

    child->Cancel();

    EXPECT_FALSE(parent->Done());
    EXPECT_EQ(parent->Err(), ContextErr::Nil);
    EXPECT_EQ(parent->Value(), 1);
}

TEST(ContextTest, WithValue_ChainedValueContextsCascadeCancel)
{
    auto root   = WithValue(1, Background());
    auto middle = WithValue(2, root);
    auto leaf   = WithValue(3, middle);

    root->Cancel();

    EXPECT_TRUE(middle->Done());
    EXPECT_TRUE(leaf->Done());
    EXPECT_EQ(middle->Value(), 2);
    EXPECT_EQ(leaf->Value(), 3);
}

// ─── ContextWithValue — Wait ──────────────────────────────────────────────────

TEST(ContextTest, WithValue_WaitReturnsImmediatelyIfAlreadyCanceled)
{
    auto ctx = WithValue(42, Background());
    ctx->Cancel();
    EXPECT_NO_THROW(ctx->Wait());
}

TEST(ContextTest, WithValue_WaitBlocksUntilCanceledFromAnotherThread)
{
    auto ctx = WithValue(42, Background());

    std::thread canceler([&ctx]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ctx->Cancel();
    });

    ctx->Wait();
    EXPECT_TRUE(ctx->Done());
    canceler.join();
}

}
