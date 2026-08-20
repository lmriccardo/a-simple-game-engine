#include <ASGE/Core/Memory/LinearAllocator.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace
{

using asge::mem::ArenaAllocator;

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, Construction_CapacityMatchesRequestedSize)
{
    ArenaAllocator arena(512);
    EXPECT_EQ(arena.Capacity(), 512u);
}

TEST(ArenaAllocatorTest, Construction_InitiallyEmpty)
{
    ArenaAllocator arena(512);
    EXPECT_TRUE(arena.Empty());
}

TEST(ArenaAllocatorTest, Construction_UsedIsZero)
{
    ArenaAllocator arena(512);
    EXPECT_EQ(arena.Used(), 0u);
}

TEST(ArenaAllocatorTest, Construction_RemainingEqualsCapacity)
{
    ArenaAllocator arena(512);
    EXPECT_EQ(arena.Remaining(), arena.Capacity());
}

// ─── Move ─────────────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, MoveConstructor_DestinationHasOriginalCapacity)
{
    ArenaAllocator src(256);
    ArenaAllocator dst(std::move(src));
    EXPECT_EQ(dst.Capacity(), 256u);
}

TEST(ArenaAllocatorTest, MoveConstructor_SourceCapacityBecomesZero)
{
    ArenaAllocator src(256);
    ArenaAllocator dst(std::move(src));
    EXPECT_EQ(src.Capacity(), 0u);
}

TEST(ArenaAllocatorTest, MoveAssignment_DestinationHasOriginalCapacity)
{
    ArenaAllocator src(256);
    ArenaAllocator dst(64);
    dst = std::move(src);
    EXPECT_EQ(dst.Capacity(), 256u);
}

TEST(ArenaAllocatorTest, MoveAssignment_SourceCapacityBecomesZero)
{
    ArenaAllocator src(256);
    ArenaAllocator dst(64);
    dst = std::move(src);
    EXPECT_EQ(src.Capacity(), 0u);
}

// ─── Alloc ────────────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, Alloc_ReturnsNonNullForValidRequest)
{
    ArenaAllocator arena(512);
    EXPECT_NE(arena.Alloc(32), nullptr);
}

TEST(ArenaAllocatorTest, Alloc_IncreasesUsed)
{
    ArenaAllocator arena(512);
    (void)arena.Alloc(64);
    EXPECT_GE(arena.Used(), 64u);
}

TEST(ArenaAllocatorTest, Alloc_UsedPlusRemainingEqualsCapacity)
{
    ArenaAllocator arena(512);
    (void)arena.Alloc(128);
    EXPECT_EQ(arena.Used() + arena.Remaining(), arena.Capacity());
}

TEST(ArenaAllocatorTest, Alloc_ReturnsNullWhenExhausted)
{
    ArenaAllocator arena(64);
    (void)arena.Alloc(64);
    EXPECT_EQ(arena.Alloc(1), nullptr);
}

// ─── AllocOne ─────────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, AllocOne_ConstructsTrivialType)
{
    ArenaAllocator arena(512);
    int* p = arena.AllocOne<int>(99);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 99);
}

TEST(ArenaAllocatorTest, AllocOne_ConstructsNonTrivialType)
{
    ArenaAllocator arena(512);
    std::string* s = arena.AllocOne<std::string>("arena");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "arena");
    s->~basic_string();
}

TEST(ArenaAllocatorTest, AllocOne_ThrowsBadAllocWhenFull)
{
    ArenaAllocator arena(sizeof(int));
    (void)arena.AllocOne<int>(0); // fill it
    EXPECT_THROW((void)arena.AllocOne<int>(0), std::bad_alloc);
}

// ─── AllocArray ───────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, AllocArray_TrivialType_ReturnsValidPointer)
{
    ArenaAllocator arena(512);
    int* arr = arena.AllocArray<int>(8);
    EXPECT_NE(arr, nullptr);
}

TEST(ArenaAllocatorTest, AllocArray_NonTrivialType_DefaultConstructed)
{
    ArenaAllocator arena(512);
    std::string* arr = arena.AllocArray<std::string>(3);
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(arr[i].empty());
    for (int i = 0; i < 3; ++i)
        arr[i].~basic_string();
}

TEST(ArenaAllocatorTest, AllocArray_ThrowsBadAllocWhenFull)
{
    ArenaAllocator arena(4);
    EXPECT_THROW((void)arena.AllocArray<int>(2), std::bad_alloc);
}

// ─── GetMarker / Rewind ───────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, GetMarker_InitialOffsetIsZero)
{
    ArenaAllocator arena(512);
    auto m = arena.GetMarker();
    EXPECT_EQ(m.s_Offset, 0u);
}

TEST(ArenaAllocatorTest, Rewind_RestoresOffset)
{
    ArenaAllocator arena(512);
    auto marker = arena.GetMarker();
    (void)arena.Alloc(128);
    arena.Rewind(marker);
    EXPECT_EQ(arena.Used(), 0u);
}

TEST(ArenaAllocatorTest, Rewind_AllowsMemoryReuse)
{
    ArenaAllocator arena(512);
    (void)arena.Alloc(64);
    auto marker = arena.GetMarker();
    int* first  = arena.AllocOne<int>(1);
    arena.Rewind(marker);
    int* second = arena.AllocOne<int>(2);
    EXPECT_EQ(first, second);
}

// ─── Reset ────────────────────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, Reset_EmptyAfterReset)
{
    ArenaAllocator arena(512);
    (void)arena.Alloc(256);
    arena.Reset();
    EXPECT_TRUE(arena.Empty());
}

TEST(ArenaAllocatorTest, Reset_AllowsReuse)
{
    ArenaAllocator arena(512);
    void* p1 = arena.Alloc(64);
    arena.Reset();
    void* p2 = arena.Alloc(64);
    EXPECT_EQ(p1, p2);
}

TEST(ArenaAllocatorTest, Reset_RemainingEqualsCapacityAfterReset)
{
    ArenaAllocator arena(512);
    (void)arena.Alloc(256);
    arena.Reset();
    EXPECT_EQ(arena.Remaining(), arena.Capacity());
}

}
