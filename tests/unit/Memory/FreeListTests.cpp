#include <ASGE/Core/Memory/FreeList.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <system_error>
#include <vector>

namespace
{

using asge::mem::FreeList;

static constexpr std::size_t kSize = 8;

// ─── Get ──────────────────────────────────────────────────────────────────────

TEST(FreeListTest, Get_FreshListReturnsOk)
{
    FreeList<kSize> list;
    auto result = list.Get();
    EXPECT_TRUE(result.IsOk());
}

TEST(FreeListTest, Get_FirstIndexIsZero)
{
    FreeList<kSize> list;
    auto result = list.Get();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0u);
}

TEST(FreeListTest, Get_ConsecutiveCallsReturnDistinctIndices)
{
    FreeList<kSize> list;
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < kSize; ++i)
    {
        auto result = list.Get();
        ASSERT_TRUE(result.IsOk());
        indices.push_back(result.Value());
    }

    std::vector<std::size_t> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < kSize; ++i)
        EXPECT_EQ(sorted[i], i);
}

TEST(FreeListTest, Get_LastAvailableSlotDoesNotCrash)
{
    // Regression test: taking the final free slot used to dereference the
    // sentinel (nullopt) node while advancing the free head.
    FreeList<1> list;
    auto result = list.Get();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 0u);
}

TEST(FreeListTest, Get_ExhaustedListReturnsError)
{
    FreeList<kSize> list;
    for (std::size_t i = 0; i < kSize; ++i)
        ASSERT_TRUE(list.Get().IsOk());

    auto result = list.Get();
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::result_out_of_range));
}

TEST(FreeListTest, Get_SingleCapacityExhaustsAfterOne)
{
    FreeList<1> list;
    ASSERT_TRUE(list.Get().IsOk());
    EXPECT_FALSE(list.Get().IsOk());
}

// ─── IsUsed ───────────────────────────────────────────────────────────────────

TEST(FreeListTest, IsUsed_UnclaimedIndexIsFalse)
{
    FreeList<kSize> list;
    EXPECT_FALSE(list.IsUsed(0));
}

TEST(FreeListTest, IsUsed_ClaimedIndexIsTrue)
{
    FreeList<kSize> list;
    auto got = list.Get();
    ASSERT_TRUE(got.IsOk());
    EXPECT_TRUE(list.IsUsed(got.Value()));
}

TEST(FreeListTest, IsUsed_FreedIndexIsFalseAgain)
{
    FreeList<kSize> list;
    auto got = list.Get();
    ASSERT_TRUE(got.IsOk());
    ASSERT_TRUE(list.Free(got.Value()).IsOk());
    EXPECT_FALSE(list.IsUsed(got.Value()));
}

TEST(FreeListTest, IsUsed_OutOfRangeIndexIsFalse)
{
    FreeList<kSize> list;
    EXPECT_FALSE(list.IsUsed(kSize + 5));
}

// ─── Free ─────────────────────────────────────────────────────────────────────

TEST(FreeListTest, Free_ValidOutstandingIndexSucceeds)
{
    FreeList<kSize> list;
    auto got = list.Get();
    ASSERT_TRUE(got.IsOk());

    auto freed = list.Free(got.Value());
    EXPECT_TRUE(freed.IsOk());
}

TEST(FreeListTest, Free_ReleasedSlotIsReusedByNextGet)
{
    FreeList<1> list;
    auto got = list.Get();
    ASSERT_TRUE(got.IsOk());
    ASSERT_TRUE(list.Free(got.Value()).IsOk());

    auto reused = list.Get();
    ASSERT_TRUE(reused.IsOk());
    EXPECT_EQ(reused.Value(), got.Value());
}

TEST(FreeListTest, Free_UnallocatedIndexIsRejected)
{
    FreeList<kSize> list;
    auto result = list.Free(3);
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST(FreeListTest, Free_DoubleFreeIsRejected)
{
    FreeList<kSize> list;
    auto got = list.Get();
    ASSERT_TRUE(got.IsOk());
    ASSERT_TRUE(list.Free(got.Value()).IsOk());

    auto secondFree = list.Free(got.Value());
    EXPECT_FALSE(secondFree.IsOk());
    EXPECT_EQ(secondFree.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST(FreeListTest, Free_OutOfRangeIndexIsRejected)
{
    FreeList<kSize> list;
    auto result = list.Free(kSize + 10);
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST(FreeListTest, Free_SentinelIndexIsRejected)
{
    // Index N is the internal end-of-list marker, not a real slot.
    FreeList<kSize> list;
    auto result = list.Free(kSize);
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST(FreeListTest, Free_AllSlotsThenFullyReallocatable)
{
    FreeList<kSize> list;
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < kSize; ++i)
        indices.push_back(list.Get().Value());

    for (auto idx : indices)
        ASSERT_TRUE(list.Free(idx).IsOk());

    for (std::size_t i = 0; i < kSize; ++i)
        EXPECT_TRUE(list.Get().IsOk());

    // Pool should be exhausted again after reclaiming exactly kSize slots.
    EXPECT_FALSE(list.Get().IsOk());
}

TEST(FreeListTest, Free_LifoReuseOrder)
{
    // Free() pushes onto the head, so the most recently freed slot is the
    // next one handed out by Get().
    FreeList<kSize> list;
    auto a = list.Get().Value();
    auto b = list.Get().Value();

    ASSERT_TRUE(list.Free(a).IsOk());
    ASSERT_TRUE(list.Free(b).IsOk());

    EXPECT_EQ(list.Get().Value(), b);
    EXPECT_EQ(list.Get().Value(), a);
}

}
