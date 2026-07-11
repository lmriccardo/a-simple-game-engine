#include <ASGE/Core/Allocators/StackAllocator.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace
{

using asge::mem::StackAllocator;

static constexpr std::size_t kBufSize = 1024;

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, Construction_InitiallyEmpty)
{
    StackAllocator<kBufSize> alloc;
    EXPECT_TRUE(alloc.Empty());
}

TEST(StackAllocatorTest, Construction_UsedIsZero)
{
    StackAllocator<kBufSize> alloc;
    EXPECT_EQ(alloc.Used(), 0u);
}

TEST(StackAllocatorTest, Construction_RemainingEqualsTotalCapacity)
{
    StackAllocator<kBufSize> alloc;
    EXPECT_EQ(alloc.Used() + alloc.Remaining(), kBufSize);
}

// ─── Alloc ────────────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, Alloc_ReturnsNonNullForFreshAllocator)
{
    StackAllocator<kBufSize> alloc;
    EXPECT_NE(alloc.Alloc<int>(0), nullptr);
}

TEST(StackAllocatorTest, Alloc_ConstructsTrivialTypeWithArgs)
{
    StackAllocator<kBufSize> alloc;
    int* p = alloc.Alloc<int>(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(StackAllocatorTest, Alloc_ConstructsNonTrivialType)
{
    StackAllocator<kBufSize> alloc;
    std::string* s = alloc.Alloc<std::string>("hello");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "hello");
}

TEST(StackAllocatorTest, Alloc_IncreasesUsed)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    EXPECT_GT(alloc.Used(), 0u);
}

TEST(StackAllocatorTest, Alloc_DecreasesRemaining)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    EXPECT_LT(alloc.Remaining(), kBufSize);
}

TEST(StackAllocatorTest, Alloc_UsedPlusRemainingEqualsCapacity)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(1);
    (void)alloc.Alloc<int>(2);
    EXPECT_EQ(alloc.Used() + alloc.Remaining(), kBufSize);
}

TEST(StackAllocatorTest, Alloc_ReturnsNullWhenExhausted)
{
    StackAllocator<64> alloc;
    while (alloc.Alloc<int>(0) != nullptr) {}
    EXPECT_EQ(alloc.Alloc<int>(0), nullptr);
}

TEST(StackAllocatorTest, Alloc_ConsecutiveAllocsReturnDistinctPointers)
{
    StackAllocator<kBufSize> alloc;
    int* p1 = alloc.Alloc<int>(1);
    int* p2 = alloc.Alloc<int>(2);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
}

TEST(StackAllocatorTest, Alloc_ValuesIndependentAcrossAllocations)
{
    StackAllocator<kBufSize> alloc;
    int* p1 = alloc.Alloc<int>(10);
    int* p2 = alloc.Alloc<int>(20);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(*p1, 10);
    EXPECT_EQ(*p2, 20);
}

TEST(StackAllocatorTest, Alloc_NotEmptyAfterAlloc)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    EXPECT_FALSE(alloc.Empty());
}

// ─── GetMarker ────────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, GetMarker_InitialMarkerIsZero)
{
    StackAllocator<kBufSize> alloc;
    EXPECT_EQ(alloc.GetMarker(), 0u);
}

TEST(StackAllocatorTest, GetMarker_AdvancesAfterAlloc)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    EXPECT_GT(alloc.GetMarker(), 0u);
}

TEST(StackAllocatorTest, GetMarker_ReflectsUsed)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    EXPECT_EQ(alloc.GetMarker(), alloc.Used());
}

TEST(StackAllocatorTest, GetMarker_CapturesUsedAfterMultipleAllocs)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    (void)alloc.Alloc<int>(0);
    std::size_t usedBefore = alloc.Used();
    auto marker = alloc.GetMarker();
    EXPECT_EQ(marker, usedBefore);
}

// ─── Free ─────────────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, Free_RestoresTopToPreviousValue)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    std::size_t usedBefore = alloc.Used();
    int* p = alloc.Alloc<int>(99);
    ASSERT_NE(p, nullptr);
    alloc.Free(p);
    EXPECT_EQ(alloc.Used(), usedBefore);
}

TEST(StackAllocatorTest, Free_DestructorCalled)
{
    static int dtorCount = 0;
    struct Tracked { ~Tracked() { ++dtorCount; } };

    dtorCount = 0;
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    Tracked* p = alloc.Alloc<Tracked>();
    ASSERT_NE(p, nullptr);
    alloc.Free(p);
    EXPECT_EQ(dtorCount, 1);
}

TEST(StackAllocatorTest, Free_AllowsMemoryReuse)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    int* p1 = alloc.Alloc<int>(1);
    ASSERT_NE(p1, nullptr);
    alloc.Free(p1);
    int* p2 = alloc.Alloc<int>(2);
    EXPECT_EQ(p1, p2);
}

// ─── FreeToMarker ─────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, FreeToMarker_NoOpWhenMarkerEqualsCurrentTop)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(1);
    auto marker      = alloc.GetMarker();
    std::size_t used = alloc.Used();
    alloc.FreeToMarker(marker);
    EXPECT_EQ(alloc.Used(), used);
}

TEST(StackAllocatorTest, FreeToMarker_RestoresTopToMarker)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    auto marker          = alloc.GetMarker();
    std::size_t usedAtMarker = alloc.Used();
    (void)alloc.Alloc<int>(1);
    (void)alloc.Alloc<int>(2);
    alloc.FreeToMarker(marker);
    EXPECT_EQ(alloc.Used(), usedAtMarker);
}

TEST(StackAllocatorTest, FreeToMarker_CallsDestructorsAboveMarker)
{
    static int dtorCount = 0;
    struct Tracked { ~Tracked() { ++dtorCount; } };

    dtorCount = 0;
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    auto marker = alloc.GetMarker();
    (void)alloc.Alloc<Tracked>();
    (void)alloc.Alloc<Tracked>();
    alloc.FreeToMarker(marker);
    EXPECT_EQ(dtorCount, 2);
}

TEST(StackAllocatorTest, FreeToMarker_AllowsMemoryReuse)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(0);
    auto marker = alloc.GetMarker();
    int* p1     = alloc.Alloc<int>(10);
    ASSERT_NE(p1, nullptr);
    alloc.FreeToMarker(marker);
    int* p2 = alloc.Alloc<int>(20);
    EXPECT_EQ(p1, p2);
}

// ─── Reset ────────────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, Reset_SetsUsedToZero)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(1);
    (void)alloc.Alloc<int>(2);
    alloc.Reset();
    EXPECT_EQ(alloc.Used(), 0u);
}

TEST(StackAllocatorTest, Reset_EmptyAfterReset)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(1);
    alloc.Reset();
    EXPECT_TRUE(alloc.Empty());
}

TEST(StackAllocatorTest, Reset_RemainingEqualsCapacityAfterReset)
{
    StackAllocator<kBufSize> alloc;
    (void)alloc.Alloc<int>(1);
    alloc.Reset();
    EXPECT_EQ(alloc.Used() + alloc.Remaining(), kBufSize);
    EXPECT_EQ(alloc.Remaining(), kBufSize);
}

TEST(StackAllocatorTest, Reset_CallsDestructorsForAllLiveObjects)
{
    static int dtorCount = 0;
    struct Tracked { ~Tracked() { ++dtorCount; } };

    dtorCount = 0;
    {
        StackAllocator<kBufSize> alloc;
        (void)alloc.Alloc<Tracked>();
        (void)alloc.Alloc<Tracked>();
        (void)alloc.Alloc<Tracked>();
        alloc.Reset();
    }
    EXPECT_EQ(dtorCount, 3);
}

TEST(StackAllocatorTest, Reset_AllowsReuse)
{
    StackAllocator<kBufSize> alloc;
    int* p1 = alloc.Alloc<int>(1);
    alloc.Reset();
    int* p2 = alloc.Alloc<int>(2);
    EXPECT_EQ(p1, p2);
}

// ─── Destructor ───────────────────────────────────────────────────────────────

TEST(StackAllocatorTest, Destructor_CallsDestructorsForAllLiveObjects)
{
    static int dtorCount = 0;
    struct Tracked { ~Tracked() { ++dtorCount; } };

    dtorCount = 0;
    {
        StackAllocator<kBufSize> alloc;
        (void)alloc.Alloc<Tracked>();
        (void)alloc.Alloc<Tracked>();
    }
    EXPECT_EQ(dtorCount, 2);
}

}
