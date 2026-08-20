#include <ASGE/Core/ECS/ComponentPool.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{

using asge::ecs::ComponentPool;
using asge::ecs::Entity;
using asge::ecs::EntityIndex;

struct Position
{
    float x{ 0.0f };
    float y{ 0.0f };

    friend bool operator==(Position const&, Position const&) = default;
};

static constexpr std::size_t kEntityCapacity    = 8;
static constexpr std::size_t kComponentCapacity = 4;

using PositionPool = ComponentPool<Position, kEntityCapacity, kComponentCapacity>;

Entity MakeEntity(std::size_t inIndex)
{
    return Entity{ static_cast<EntityIndex>(inIndex), 0 };
}

// ─── Insert ───────────────────────────────────────────────────────────────────

TEST(ComponentPoolTest, Insert_FreshEntityReturnsOk)
{
    PositionPool pool;
    auto result = pool.Insert(MakeEntity(0), Position{ 1.0f, 2.0f });
    EXPECT_TRUE(result.IsOk());
}

TEST(ComponentPoolTest, Insert_ReturnedReferenceHoldsValue)
{
    PositionPool pool;
    auto result = pool.Insert(MakeEntity(0), Position{ 1.0f, 2.0f });
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get(), (Position{ 1.0f, 2.0f }));
}

TEST(ComponentPoolTest, Insert_OutOfRangeEntityIsRejected)
{
    PositionPool pool;
    auto result = pool.Insert(MakeEntity(kEntityCapacity), Position{});
    EXPECT_FALSE(result.IsOk());
}

TEST(ComponentPoolTest, Insert_ExhaustedPoolIsRejected)
{
    PositionPool pool;
    for (std::size_t i = 0; i < kComponentCapacity; ++i)
        ASSERT_TRUE(pool.Insert(MakeEntity(i), Position{}).IsOk());

    auto result = pool.Insert(MakeEntity(kComponentCapacity), Position{});
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::NoMoreComponentsAvailable));
}

TEST(ComponentPoolTest, Insert_SameEntityTwiceReplacesValue)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{ 1.0f, 1.0f }).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{ 9.0f, 9.0f }).IsOk());

    auto result = pool.Get(MakeEntity(0));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get(), (Position{ 9.0f, 9.0f }));
}

TEST(ComponentPoolTest, Insert_ReinsertingSameEntityDoesNotConsumeExtraCapacity)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());

    // Only one slot was ever consumed, so kComponentCapacity - 1 more distinct
    // entities should still fit.
    for (std::size_t i = 1; i < kComponentCapacity; ++i)
        EXPECT_TRUE(pool.Insert(MakeEntity(i), Position{}).IsOk());
}

// ─── Contains ─────────────────────────────────────────────────────────────────

TEST(ComponentPoolTest, Contains_InsertedEntityIsTrue)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(2), Position{}).IsOk());
    EXPECT_TRUE(pool.Contains(MakeEntity(2)));
}

TEST(ComponentPoolTest, Contains_UninsertedEntityIsFalse)
{
    PositionPool pool;
    EXPECT_FALSE(pool.Contains(MakeEntity(2)));
}

TEST(ComponentPoolTest, Contains_OutOfRangeEntityIsFalse)
{
    PositionPool pool;
    EXPECT_FALSE(pool.Contains(MakeEntity(kEntityCapacity + 5)));
}

// ─── Get ──────────────────────────────────────────────────────────────────────

TEST(ComponentPoolTest, Get_InsertedEntityReturnsStoredValue)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(1), Position{ 3.0f, 4.0f }).IsOk());

    auto result = pool.Get(MakeEntity(1));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get(), (Position{ 3.0f, 4.0f }));
}

TEST(ComponentPoolTest, Get_MissingEntityReturnsError)
{
    PositionPool pool;
    auto result = pool.Get(MakeEntity(1));
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::EntityNotAttachedToComponent));
}

TEST(ComponentPoolTest, Get_ReturnedReferenceIsMutable)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{ 1.0f, 1.0f }).IsOk());

    auto result = pool.Get(MakeEntity(0));
    ASSERT_TRUE(result.IsOk());
    result.Value().get().x = 42.0f;

    auto second = pool.Get(MakeEntity(0));
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(second.Value().get().x, 42.0f);
}

// ─── Remove ───────────────────────────────────────────────────────────────────

TEST(ComponentPoolTest, Remove_ExistingEntitySucceeds)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());
    EXPECT_TRUE(pool.Remove(MakeEntity(0)).IsOk());
}

TEST(ComponentPoolTest, Remove_MissingEntityIsRejected)
{
    PositionPool pool;
    auto result = pool.Remove(MakeEntity(0));
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::EntityNotAttachedToComponent));
}

TEST(ComponentPoolTest, Remove_RemovedEntityNoLongerContained)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());
    ASSERT_TRUE(pool.Remove(MakeEntity(0)).IsOk());
    EXPECT_FALSE(pool.Contains(MakeEntity(0)));
}

TEST(ComponentPoolTest, Remove_DoubleRemoveIsRejected)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());
    ASSERT_TRUE(pool.Remove(MakeEntity(0)).IsOk());

    EXPECT_FALSE(pool.Remove(MakeEntity(0)).IsOk());
}

TEST(ComponentPoolTest, Remove_SwapPopPreservesOtherEntities)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{ 1.0f, 0.0f }).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(1), Position{ 2.0f, 0.0f }).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(2), Position{ 3.0f, 0.0f }).IsOk());

    // Remove the middle entity, forcing the swap-and-pop to relocate entity 2.
    ASSERT_TRUE(pool.Remove(MakeEntity(1)).IsOk());

    EXPECT_FALSE(pool.Contains(MakeEntity(1)));

    auto e0 = pool.Get(MakeEntity(0));
    ASSERT_TRUE(e0.IsOk());
    EXPECT_EQ(e0.Value().get(), (Position{ 1.0f, 0.0f }));

    auto e2 = pool.Get(MakeEntity(2));
    ASSERT_TRUE(e2.IsOk());
    EXPECT_EQ(e2.Value().get(), (Position{ 3.0f, 0.0f }));
}

TEST(ComponentPoolTest, Remove_FreesSlotForReinsertion)
{
    PositionPool pool;
    for (std::size_t i = 0; i < kComponentCapacity; ++i)
        ASSERT_TRUE(pool.Insert(MakeEntity(i), Position{}).IsOk());

    ASSERT_TRUE(pool.Remove(MakeEntity(0)).IsOk());

    // A previously-exhausted pool should accept one more insertion now.
    EXPECT_TRUE(pool.Insert(MakeEntity(kComponentCapacity), Position{}).IsOk());
}

// ─── Iteration ────────────────────────────────────────────────────────────────

TEST(ComponentPoolTest, Iteration_EmptyPoolHasNoElements)
{
    PositionPool pool;
    std::size_t count = 0;
    for (auto it = pool.begin(); it != pool.end(); ++it)
        ++count;

    EXPECT_EQ(count, 0u);
}

TEST(ComponentPoolTest, Iteration_VisitsAllInsertedPairs)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{ 1.0f, 0.0f }).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(1), Position{ 2.0f, 0.0f }).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(2), Position{ 3.0f, 0.0f }).IsOk());

    std::vector<EntityIndex> visited;
    for (auto it = pool.begin(); it != pool.end(); ++it)
    {
        auto [entity, comp] = *it;
        visited.push_back(entity.m_Index);
        (void)comp;
    }

    std::sort(visited.begin(), visited.end());
    EXPECT_EQ(visited, (std::vector<EntityIndex>{ 0, 1, 2 }));
}

TEST(ComponentPoolTest, Iteration_ReflectsRemoval)
{
    PositionPool pool;
    ASSERT_TRUE(pool.Insert(MakeEntity(0), Position{}).IsOk());
    ASSERT_TRUE(pool.Insert(MakeEntity(1), Position{}).IsOk());
    ASSERT_TRUE(pool.Remove(MakeEntity(0)).IsOk());

    std::size_t count = 0;
    for (auto it = pool.begin(); it != pool.end(); ++it)
        ++count;

    EXPECT_EQ(count, 1u);
}

}
