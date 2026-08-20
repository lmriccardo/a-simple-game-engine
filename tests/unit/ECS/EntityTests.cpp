#include <ASGE/Core/ECS/Entity.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <system_error>
#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::EntityAllocator;
using asge::ecs::EntityIndex;

static constexpr std::size_t kSize = 8;

// ─── Entity ───────────────────────────────────────────────────────────────────

TEST(EntityTest, Equality_SameIndexAndGenerationAreEqual)
{
    Entity a{ 3, 1 };
    Entity b{ 3, 1 };
    EXPECT_EQ(a, b);
}

TEST(EntityTest, Equality_DifferentGenerationAreNotEqual)
{
    Entity a{ 3, 1 };
    Entity b{ 3, 2 };
    EXPECT_NE(a, b);
}

TEST(EntityTest, Null_HasMaxIndex)
{
    Entity null = Entity::Null();
    EXPECT_EQ(null.m_Index, std::numeric_limits<EntityIndex>::max());
}

// ─── EntityAllocator::Create ────────────────────────────────────────────────

TEST(EntityAllocatorTest, Create_FreshAllocatorReturnsOk)
{
    EntityAllocator<kSize> alloc;
    auto result = alloc.Create();
    EXPECT_TRUE(result.IsOk());
}

TEST(EntityAllocatorTest, Create_FirstEntityHasGenerationZero)
{
    EntityAllocator<kSize> alloc;
    auto result = alloc.Create();
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().m_Generation, 0u);
}

TEST(EntityAllocatorTest, Create_ConsecutiveEntitiesHaveDistinctIndices)
{
    EntityAllocator<kSize> alloc;
    std::vector<EntityIndex> indices;
    for (std::size_t i = 0; i < kSize; ++i)
    {
        auto result = alloc.Create();
        ASSERT_TRUE(result.IsOk());
        indices.push_back(result.Value().m_Index);
    }

    std::vector<EntityIndex> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < kSize; ++i)
        EXPECT_EQ(sorted[i], static_cast<EntityIndex>(i));
}

TEST(EntityAllocatorTest, Create_ExhaustedAllocatorReturnsError)
{
    EntityAllocator<kSize> alloc;
    for (std::size_t i = 0; i < kSize; ++i)
        ASSERT_TRUE(alloc.Create().IsOk());

    auto result = alloc.Create();
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::NoMoreEntityAvailable));
}

TEST(EntityAllocatorTest, Create_ReturnedEntityIsAlive)
{
    EntityAllocator<kSize> alloc;
    auto result = alloc.Create();
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(alloc.IsAlive(result.Value()));
}

// ─── EntityAllocator::IsAlive ───────────────────────────────────────────────

TEST(EntityAllocatorTest, IsAlive_NullEntityIsNotAlive)
{
    EntityAllocator<kSize> alloc;
    EXPECT_FALSE(alloc.IsAlive(Entity::Null()));
}

TEST(EntityAllocatorTest, IsAlive_OutOfRangeIndexIsNotAlive)
{
    EntityAllocator<kSize> alloc;
    EXPECT_FALSE(alloc.IsAlive(Entity{ kSize, 0 }));
}

TEST(EntityAllocatorTest, IsAlive_StaleGenerationIsNotAlive)
{
    EntityAllocator<kSize> alloc;
    auto created = alloc.Create();
    ASSERT_TRUE(created.IsOk());
    Entity handle = created.Value();

    ASSERT_TRUE(alloc.Destroy(handle).IsOk());
    EXPECT_FALSE(alloc.IsAlive(handle));
}

TEST(EntityAllocatorTest, IsAlive_NeverAllocatedIndexIsNotAlive)
{
    // Slot 0's generation defaults to 0, same as Entity{0, 0} — IsAlive()
    // must rely on the slot actually being claimed, not just the generation
    // matching, or a hand-built handle for an untouched slot would pass.
    EntityAllocator<kSize> alloc;
    EXPECT_FALSE(alloc.IsAlive(Entity{ 0, 0 }));
}

// ─── EntityAllocator::Destroy ───────────────────────────────────────────────

TEST(EntityAllocatorTest, Destroy_LiveEntitySucceeds)
{
    EntityAllocator<kSize> alloc;
    auto created = alloc.Create();
    ASSERT_TRUE(created.IsOk());

    auto destroyed = alloc.Destroy(created.Value());
    EXPECT_TRUE(destroyed.IsOk());
}

TEST(EntityAllocatorTest, Destroy_NeverAllocatedIndexIsRejected)
{
    // Entity{0, 0} was never returned by Create() — Destroy() must refuse it
    // even though slot 0's generation happens to default to 0 as well.
    EntityAllocator<kSize> alloc;
    auto result = alloc.Destroy(Entity{ 0, 0 });
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::EntityIsNotAlive));
}

TEST(EntityAllocatorTest, Destroy_OutOfRangeIndexIsRejected)
{
    EntityAllocator<kSize> alloc;
    auto result = alloc.Destroy(Entity{ kSize + 3, 0 });
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::EntityIsNotAlive));
}

TEST(EntityAllocatorTest, Destroy_DoubleDestroyIsRejected)
{
    EntityAllocator<kSize> alloc;
    auto created = alloc.Create();
    ASSERT_TRUE(created.IsOk());
    Entity handle = created.Value();

    ASSERT_TRUE(alloc.Destroy(handle).IsOk());

    auto secondDestroy = alloc.Destroy(handle);
    EXPECT_FALSE(secondDestroy.IsOk());
    EXPECT_EQ(secondDestroy.Code(), make_error_code(asge::errors::EcsError::EntityIsNotAlive));
}

TEST(EntityAllocatorTest, Destroy_RecycledSlotBumpsGeneration)
{
    EntityAllocator<1> alloc;
    auto first = alloc.Create();
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(alloc.Destroy(first.Value()).IsOk());

    auto second = alloc.Create();
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(second.Value().m_Index, first.Value().m_Index);
    EXPECT_EQ(second.Value().m_Generation, first.Value().m_Generation + 1);
}

TEST(EntityAllocatorTest, Destroy_StaleHandleRejectedAfterSlotRecycled)
{
    EntityAllocator<1> alloc;
    auto first = alloc.Create();
    ASSERT_TRUE(first.IsOk());
    Entity staleHandle = first.Value();
    ASSERT_TRUE(alloc.Destroy(staleHandle).IsOk());

    auto second = alloc.Create();
    ASSERT_TRUE(second.IsOk());

    // The old handle must not be mistaken for the new occupant of the slot.
    EXPECT_FALSE(alloc.IsAlive(staleHandle));
    auto result = alloc.Destroy(staleHandle);
    EXPECT_FALSE(result.IsOk());
}

TEST(EntityAllocatorTest, Destroy_ReleasedSlotAllowsFullReallocation)
{
    EntityAllocator<kSize> alloc;
    std::vector<Entity> handles;
    for (std::size_t i = 0; i < kSize; ++i)
        handles.push_back(alloc.Create().Value());

    for (auto const& h : handles)
        ASSERT_TRUE(alloc.Destroy(h).IsOk());

    for (std::size_t i = 0; i < kSize; ++i)
        EXPECT_TRUE(alloc.Create().IsOk());

    // Exhausted again after reclaiming exactly kSize slots.
    EXPECT_FALSE(alloc.Create().IsOk());
}

}
