#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::EntityIndex;
using asge::ecs::Registry;

struct Position
{
    float x{ 0.0f };
    float y{ 0.0f };

    friend bool operator==(Position const&, Position const&) = default;
};

struct Velocity
{
    float dx{ 0.0f };
    float dy{ 0.0f };

    friend bool operator==(Velocity const&, Velocity const&) = default;
};

// ─── Registry::View — emptiness ────────────────────────────────────────────────

TEST(RegistryTest, View_EmptyRegistryYieldsEmptyView)
{
    Registry registry;
    auto view = registry.View<Position>();

    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

TEST(RegistryTest, View_NeverUsedComponentTypeIsEmpty)
{
    Registry registry;
    auto e = registry.CreateEntity();
    ASSERT_TRUE(e.IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(e.Value(), Position{ 1.0f, 2.0f }).IsOk());

    // Velocity has never been added to any entity, so its pool was never
    // created — the view must stay empty even though Position has data.
    auto view = registry.View<Position, Velocity>();

    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

// ─── Registry::View — single component ─────────────────────────────────────────

TEST(RegistryTest, View_SingleComponentVisitsAllEntitiesWithIt)
{
    Registry registry;
    for (int i = 0; i < 3; ++i)
    {
        auto e = registry.CreateEntity();
        ASSERT_TRUE(e.IsOk());
        ASSERT_TRUE(registry.AddComponent<Position>(e.Value(), Position{ float(i), 0.0f }).IsOk());
    }

    std::vector<float> seen;
    for (auto [entity, pos] : registry.View<Position>())
    {
        (void)entity;
        seen.push_back(pos.get().x);
    }

    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen, (std::vector<float>{ 0.0f, 1.0f, 2.0f }));
}

TEST(RegistryTest, View_ReturnedReferenceIsMutable)
{
    Registry registry;
    auto e = registry.CreateEntity();
    ASSERT_TRUE(e.IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(e.Value(), Position{ 1.0f, 1.0f }).IsOk());

    for (auto [entity, pos] : registry.View<Position>())
    {
        (void)entity;
        pos.get().x = 42.0f;
    }

    auto result = registry.GetComponent<Position>(e.Value());
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get().x, 42.0f);
}

// ─── Registry::View — intersection across multiple components ─────────────────

TEST(RegistryTest, View_MultipleComponentsOnlyVisitsEntitiesWithAll)
{
    Registry registry;
    auto both = registry.CreateEntity().Value();
    auto onlyPosition = registry.CreateEntity().Value();

    ASSERT_TRUE(registry.AddComponent<Position>(both, Position{ 1.0f, 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(both, Velocity{ 2.0f, 2.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(onlyPosition, Position{ 9.0f, 9.0f }).IsOk());

    std::vector<EntityIndex> seen;
    for (auto [entity, pos, vel] : registry.View<Position, Velocity>())
    {
        (void)pos;
        (void)vel;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], both.m_Index);
}

TEST(RegistryTest, View_MultipleComponentsYieldsCorrectValuesForEachType)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{ 3.0f, 4.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(e, Velocity{ 5.0f, 6.0f }).IsOk());

    bool visited = false;
    for (auto [entity, pos, vel] : registry.View<Position, Velocity>())
    {
        EXPECT_EQ(entity, e);
        EXPECT_EQ(pos.get(), (Position{ 3.0f, 4.0f }));
        EXPECT_EQ(vel.get(), (Velocity{ 5.0f, 6.0f }));
        visited = true;
    }
    EXPECT_TRUE(visited);
}

TEST(RegistryTest, View_NoEntityHasAllComponentsIsEmpty)
{
    Registry registry;
    auto e1 = registry.CreateEntity().Value();
    auto e2 = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e1, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(e2, Velocity{}).IsOk());

    auto view = registry.View<Position, Velocity>();
    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

// ─── Registry::View — reflects mutation to the registry ────────────────────────

TEST(RegistryTest, View_ExcludesEntityAfterComponentRemoved)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(e, Velocity{}).IsOk());
    ASSERT_TRUE(registry.RemoveComponent<Velocity>(e).IsOk());

    auto view = registry.View<Position, Velocity>();
    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

TEST(RegistryTest, View_ExcludesEntityAfterDestroy)
{
    Registry registry;
    auto keep = registry.CreateEntity().Value();
    auto destroyed = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(keep, Position{ 1.0f, 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(destroyed, Position{ 2.0f, 2.0f }).IsOk());
    ASSERT_TRUE(registry.DestroyEntity(destroyed).IsOk());

    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : registry.View<Position>())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], keep.m_Index);
}

// ─── Registry::AllEntities — unfiltered by component type ─────────────────────

TEST(RegistryTest, AllEntities_EmptyRegistryIsEmpty)
{
    Registry registry;
    EXPECT_TRUE(registry.AllEntities().empty());
}

TEST(RegistryTest, AllEntities_IncludesEntityWithNoComponents)
{
    // The whole point vs. View<Ts...>: an entity with zero components is
    // still visible here, since nothing filters by pool membership.
    Registry registry;
    auto e = registry.CreateEntity().Value();

    auto all = registry.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], e);
}

TEST(RegistryTest, AllEntities_ListsEveryLiveEntityRegardlessOfComponents)
{
    Registry registry;
    auto withPosition = registry.CreateEntity().Value();
    auto withBoth      = registry.CreateEntity().Value();
    auto bare          = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(withPosition, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(withBoth, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(withBoth, Velocity{}).IsOk());

    auto all = registry.AllEntities();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_NE(std::find(all.begin(), all.end(), withPosition), all.end());
    EXPECT_NE(std::find(all.begin(), all.end(), withBoth), all.end());
    EXPECT_NE(std::find(all.begin(), all.end(), bare), all.end());
}

TEST(RegistryTest, AllEntities_ExcludesDestroyedEntities)
{
    Registry registry;
    auto keep = registry.CreateEntity().Value();
    auto destroyed = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.DestroyEntity(destroyed).IsOk());

    auto all = registry.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], keep);
}

TEST(RegistryTest, AllEntities_RecycledSlotShowsOnlyTheNewGeneration)
{
    // Destroying and recreating reuses the freed slot index with a bumped
    // generation — AllEntities must reflect the live handle, not the stale one.
    Registry registry;
    auto original = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.DestroyEntity(original).IsOk());
    auto recycled = registry.CreateEntity().Value();

    ASSERT_EQ(recycled.m_Index, original.m_Index);
    ASSERT_NE(recycled.m_Generation, original.m_Generation);

    auto all = registry.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], recycled);
}

}
