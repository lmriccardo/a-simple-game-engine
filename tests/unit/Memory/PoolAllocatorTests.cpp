#include <ASGE/Core/Memory/PoolAllocator.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{

using asge::mem::PoolAllocator;

static constexpr std::size_t kPoolSize = 8;

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, Construction_CreateReturnsNonNull)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    EXPECT_NE(pool, nullptr);
}

// ─── Alloc ────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, Alloc_ReturnsNonNullForFreshPool)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    auto ptr  = pool->Alloc(42);
    EXPECT_NE(ptr, nullptr);
}

TEST(PoolAllocatorTest, Alloc_ConstructsTrivialTypeWithArgs)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    auto ptr  = pool->Alloc(99);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 99);
}

TEST(PoolAllocatorTest, Alloc_ConstructsNonTrivialType)
{
    auto pool = PoolAllocator<std::string, kPoolSize>::Create();
    auto ptr  = pool->Alloc("hello");
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, "hello");
}

TEST(PoolAllocatorTest, Alloc_ConsecutiveAllocsReturnNonNull)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    std::vector<PoolAllocator<int, kPoolSize>::pointer> ptrs;
    ptrs.reserve(kPoolSize);
    for (std::size_t i = 0; i < kPoolSize; ++i)
        ptrs.push_back(pool->Alloc(static_cast<int>(i)));

    for (auto& p : ptrs)
        EXPECT_NE(p, nullptr);
}

TEST(PoolAllocatorTest, Alloc_ReturnsNullWhenPoolExhausted)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    std::vector<PoolAllocator<int, kPoolSize>::pointer> ptrs;
    ptrs.reserve(kPoolSize);
    for (std::size_t i = 0; i < kPoolSize; ++i)
        ptrs.push_back(pool->Alloc(static_cast<int>(i)));

    auto extra = pool->Alloc(0);
    EXPECT_EQ(extra, nullptr);
}

TEST(PoolAllocatorTest, Alloc_DistinctAllocsDoNotAlias)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    auto p1   = pool->Alloc(1);
    auto p2   = pool->Alloc(2);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1.get(), p2.get());
}

TEST(PoolAllocatorTest, Alloc_ValuesIndependentAcrossSlots)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    std::vector<PoolAllocator<int, kPoolSize>::pointer> ptrs;
    ptrs.reserve(kPoolSize);
    for (std::size_t i = 0; i < kPoolSize; ++i)
        ptrs.push_back(pool->Alloc(static_cast<int>(i)));

    for (std::size_t i = 0; i < kPoolSize; ++i)
    {
        ASSERT_NE(ptrs[i], nullptr);
        EXPECT_EQ(*ptrs[i], static_cast<int>(i));
    }
}

// ─── Free (via Deleter) ───────────────────────────────────────────────────────

TEST(PoolAllocatorTest, Free_ReleasedSlotAllowsNewAllocation)
{
    auto pool = PoolAllocator<int, 1>::Create();
    {
        auto ptr = pool->Alloc(10);
        ASSERT_NE(ptr, nullptr);
    } // Deleter fires here, slot returned to pool
    auto ptr2 = pool->Alloc(20);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 20);
}

TEST(PoolAllocatorTest, Free_AllSlotsRecyclableAfterFullRelease)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();
    {
        std::vector<PoolAllocator<int, kPoolSize>::pointer> ptrs;
        ptrs.reserve(kPoolSize);
        for (std::size_t i = 0; i < kPoolSize; ++i)
            ptrs.push_back(pool->Alloc(static_cast<int>(i)));
    } // all ptrs freed here
    for (std::size_t i = 0; i < kPoolSize; ++i)
    {
        auto ptr = pool->Alloc(static_cast<int>(i));
        EXPECT_NE(ptr, nullptr);
    }
}

TEST(PoolAllocatorTest, Free_DestructorCalledOnNonTrivialType)
{
    static int dtorCount = 0;
    struct Tracked
    {
        ~Tracked() { ++dtorCount; }
    };

    dtorCount = 0;
    auto pool = PoolAllocator<Tracked, kPoolSize>::Create();
    {
        auto ptr = pool->Alloc();
        ASSERT_NE(ptr, nullptr);
    }
    EXPECT_EQ(dtorCount, 1);
}

TEST(PoolAllocatorTest, Free_PartialReleaseAllowsExactReallocCount)
{
    auto pool = PoolAllocator<int, kPoolSize>::Create();

    std::vector<PoolAllocator<int, kPoolSize>::pointer> held;
    held.reserve(kPoolSize / 2);
    for (std::size_t i = 0; i < kPoolSize / 2; ++i)
        held.push_back(pool->Alloc(static_cast<int>(i)));

    {
        std::vector<PoolAllocator<int, kPoolSize>::pointer> tmp;
        tmp.reserve(kPoolSize / 2);
        for (std::size_t i = 0; i < kPoolSize / 2; ++i)
            tmp.push_back(pool->Alloc(static_cast<int>(i)));
    } // tmp freed; kPoolSize/2 slots returned

    for (std::size_t i = 0; i < kPoolSize / 2; ++i)
    {
        auto ptr = pool->Alloc(static_cast<int>(i));
        EXPECT_NE(ptr, nullptr);
    }
}

// ─── Pool lifetime ────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, PoolLifetime_OutstandingPtrKeepsPoolAlive)
{
    std::weak_ptr<PoolAllocator<int, kPoolSize>> weakPool;
    PoolAllocator<int, kPoolSize>::pointer ptr;

    {
        auto pool = PoolAllocator<int, kPoolSize>::Create();
        weakPool  = pool;
        ptr       = pool->Alloc(7);
    } // pool shared_ptr dropped

    // pool is still alive because Deleter holds a copy
    EXPECT_FALSE(weakPool.expired());

    ptr.reset(); // release the UniquePtr → Deleter runs → pool refcount hits 0

    // now the pool is truly gone
    EXPECT_TRUE(weakPool.expired());
}

}
