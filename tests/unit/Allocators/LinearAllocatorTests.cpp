#include <ASGE/Core/Allocators/LinearAllocator.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace
{

using asge::mem::LinearAllocator;

static constexpr std::size_t kBufSize = 1024;

#define MAKE_ALLOC()                                                       \
    alignas(alignof(std::max_align_t)) std::byte _buf[kBufSize]{};        \
    LinearAllocator alloc(_buf, kBufSize)

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, Construction_CapacityMatchesBufferSize)
{
    MAKE_ALLOC();
    EXPECT_EQ(alloc.Capacity(), kBufSize);
}

TEST(LinearAllocatorTest, Construction_InitiallyEmpty)
{
    MAKE_ALLOC();
    EXPECT_TRUE(alloc.Empty());
}

TEST(LinearAllocatorTest, Construction_UsedIsZero)
{
    MAKE_ALLOC();
    EXPECT_EQ(alloc.Used(), 0u);
}

TEST(LinearAllocatorTest, Construction_RemainingEqualsCapacity)
{
    MAKE_ALLOC();
    EXPECT_EQ(alloc.Remaining(), kBufSize);
}

// ─── Move constructor ─────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, MoveConstructor_SourceCapacityBecomesZero)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(32);
    LinearAllocator moved(std::move(alloc));
    EXPECT_EQ(alloc.Capacity(), 0u);
    EXPECT_EQ(alloc.Used(),     0u);
}

TEST(LinearAllocatorTest, MoveConstructor_DestinationHasOriginalCapacity)
{
    MAKE_ALLOC();
    LinearAllocator moved(std::move(alloc));
    EXPECT_EQ(moved.Capacity(), kBufSize);
}

// ─── Alloc ────────────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, Alloc_ReturnsNonNullForValidRequest)
{
    MAKE_ALLOC();
    EXPECT_NE(alloc.Alloc(16), nullptr);
}

TEST(LinearAllocatorTest, Alloc_PointerIsWithinBuffer)
{
    MAKE_ALLOC();
    auto* p = static_cast<std::byte*>(alloc.Alloc(64));
    EXPECT_GE(p, _buf);
    EXPECT_LT(p, _buf + kBufSize);
}

TEST(LinearAllocatorTest, Alloc_IncreasesUsed)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    EXPECT_GE(alloc.Used(), 64u);
}

TEST(LinearAllocatorTest, Alloc_DecreasesRemaining)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    EXPECT_LE(alloc.Remaining(), kBufSize - 64u);
}

TEST(LinearAllocatorTest, Alloc_UsedPlusRemainingEqualsCapacity)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(128);
    EXPECT_EQ(alloc.Used() + alloc.Remaining(), alloc.Capacity());
}

TEST(LinearAllocatorTest, Alloc_ExactFillReturnsValidPointer)
{
    MAKE_ALLOC();
    void* p = alloc.Alloc(kBufSize);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(alloc.Remaining(), 0u);
}

TEST(LinearAllocatorTest, Alloc_OverCapacityReturnsNull)
{
    MAKE_ALLOC();
    EXPECT_EQ(alloc.Alloc(kBufSize + 1), nullptr);
}

TEST(LinearAllocatorTest, Alloc_ReturnsNullWhenExhausted)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(kBufSize);
    EXPECT_EQ(alloc.Alloc(1), nullptr);
}

TEST(LinearAllocatorTest, Alloc_ConsecutiveAllocsDoNotOverlap)
{
    MAKE_ALLOC();
    auto* p1 = static_cast<std::byte*>(alloc.Alloc(8, 1));
    auto* p2 = static_cast<std::byte*>(alloc.Alloc(8, 1));
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_GE(p2, p1 + 8);
}

TEST(LinearAllocatorTest, Alloc_AlignmentRespected)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(1, 1); // nudge cursor off the natural alignment
    void* p = alloc.Alloc(8);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t), 0u);
}

// ─── AllocOne ─────────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, AllocOne_ConstructsTrivialType)
{
    MAKE_ALLOC();
    int* p = alloc.AllocOne<int>(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(LinearAllocatorTest, AllocOne_ForwardsConstructorArgs)
{
    MAKE_ALLOC();
    struct Point { int x, y; };
    Point* pt = alloc.AllocOne<Point>(Point{3, 7});
    ASSERT_NE(pt, nullptr);
    EXPECT_EQ(pt->x, 3);
    EXPECT_EQ(pt->y, 7);
}

TEST(LinearAllocatorTest, AllocOne_ConstructsNonTrivialType)
{
    MAKE_ALLOC();
    std::string* s = alloc.AllocOne<std::string>("hello");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "hello");
    s->~basic_string();
}

TEST(LinearAllocatorTest, AllocOne_ThrowsBadAllocWhenFull)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(kBufSize);
    EXPECT_THROW((void)alloc.AllocOne<int>(0), std::bad_alloc);
}

// ─── AllocArray ───────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, AllocArray_TrivialType_ReturnsValidPointer)
{
    MAKE_ALLOC();
    int* arr = alloc.AllocArray<int>(10);
    EXPECT_NE(arr, nullptr);
}

TEST(LinearAllocatorTest, AllocArray_TrivialType_SizeReflected)
{
    MAKE_ALLOC();
    (void)alloc.AllocArray<int>(10);
    EXPECT_GE(alloc.Used(), 10 * sizeof(int));
}

TEST(LinearAllocatorTest, AllocArray_NonTrivialType_DefaultConstructed)
{
    MAKE_ALLOC();
    std::string* arr = alloc.AllocArray<std::string>(3);
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(arr[i].empty());
    for (int i = 0; i < 3; ++i)
        arr[i].~basic_string();
}

TEST(LinearAllocatorTest, AllocArray_ThrowsBadAllocWhenFull)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(kBufSize);
    EXPECT_THROW((void)alloc.AllocArray<int>(1), std::bad_alloc);
}

// ─── GetMarker / Rewind ───────────────────────────────────────────────────────

TEST(LinearAllocatorTest, GetMarker_InitialMarkerOffsetIsZero)
{
    MAKE_ALLOC();
    auto m = alloc.GetMarker();
    EXPECT_EQ(m.s_Offset, 0u);
}

TEST(LinearAllocatorTest, GetMarker_CapturesOffsetAfterAlloc)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    auto m = alloc.GetMarker();
    EXPECT_GE(m.s_Offset, 64u);
}

TEST(LinearAllocatorTest, Rewind_RestoresOffsetToMarker)
{
    MAKE_ALLOC();
    auto marker = alloc.GetMarker();
    (void)alloc.Alloc(128);
    alloc.Rewind(marker);
    EXPECT_EQ(alloc.Used(), 0u);
}

TEST(LinearAllocatorTest, Rewind_MidpointMarkerRestoresCorrectly)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    auto marker = alloc.GetMarker();
    std::size_t usedAtMarker = alloc.Used();
    (void)alloc.Alloc(128);
    alloc.Rewind(marker);
    EXPECT_EQ(alloc.Used(), usedAtMarker);
}

TEST(LinearAllocatorTest, Rewind_AllowsMemoryReuse)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    auto marker = alloc.GetMarker();
    int* first  = alloc.AllocOne<int>(1);
    alloc.Rewind(marker);
    int* second = alloc.AllocOne<int>(2);
    EXPECT_EQ(first, second);
}

// ─── Reset ────────────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, Reset_SetsUsedToZero)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(256);
    alloc.Reset();
    EXPECT_EQ(alloc.Used(), 0u);
}

TEST(LinearAllocatorTest, Reset_EmptyAfterReset)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(256);
    alloc.Reset();
    EXPECT_TRUE(alloc.Empty());
}

TEST(LinearAllocatorTest, Reset_RemainingEqualsCapacityAfterReset)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(256);
    alloc.Reset();
    EXPECT_EQ(alloc.Remaining(), alloc.Capacity());
}

TEST(LinearAllocatorTest, Reset_AllowsReuse)
{
    MAKE_ALLOC();
    void* p1 = alloc.Alloc(64);
    alloc.Reset();
    void* p2 = alloc.Alloc(64);
    EXPECT_EQ(p1, p2);
}

// ─── Empty ────────────────────────────────────────────────────────────────────

TEST(LinearAllocatorTest, Empty_TrueWhenFresh)
{
    MAKE_ALLOC();
    EXPECT_TRUE(alloc.Empty());
}

TEST(LinearAllocatorTest, Empty_FalseAfterAlloc)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(1, 1);
    EXPECT_FALSE(alloc.Empty());
}

TEST(LinearAllocatorTest, Empty_TrueAfterReset)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(64);
    alloc.Reset();
    EXPECT_TRUE(alloc.Empty());
}

// ─── Capacity / Used / Remaining consistency ──────────────────────────────────

TEST(LinearAllocatorTest, Capacity_IsConstantAcrossAllocs)
{
    MAKE_ALLOC();
    std::size_t cap = alloc.Capacity();
    (void)alloc.Alloc(32);
    (void)alloc.Alloc(64);
    EXPECT_EQ(alloc.Capacity(), cap);
}

TEST(LinearAllocatorTest, UsedPlusRemainingEqualsCapacity_AfterMultipleAllocs)
{
    MAKE_ALLOC();
    (void)alloc.Alloc(32);
    (void)alloc.Alloc(64);
    EXPECT_EQ(alloc.Used() + alloc.Remaining(), alloc.Capacity());
}

}
