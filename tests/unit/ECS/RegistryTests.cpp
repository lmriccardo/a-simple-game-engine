#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/ECS/Markers.hpp>
#include <ASGE/Core/ECS/Hierarchy.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::EntityIndex;
using asge::ecs::Registry;
using asge::ecs::markers::Disable;
using asge::ecs::components::AttachChild;
using asge::ecs::components::DetachChild;

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

struct GameSettings
{
    int m_MaxPlayers{ 4 };

    friend bool operator==(GameSettings const&, GameSettings const&) = default;
};

struct GameSpeed
{
    float m_Multiplier{ 1.0f };

    friend bool operator==(GameSpeed const&, GameSpeed const&) = default;
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

// ─── Registry::View — const overload ───────────────────────────────────────────

TEST(RegistryTest, View_ConstRegistryVisitsAllEntitiesWithIt)
{
    Registry registry;
    for (int i = 0; i < 3; ++i)
    {
        auto e = registry.CreateEntity();
        ASSERT_TRUE(e.IsOk());
        ASSERT_TRUE(registry.AddComponent<Position>(e.Value(), Position{ float(i), 0.0f }).IsOk());
    }

    Registry const& constRegistry = registry;
    std::vector<float> seen;
    for (auto [entity, pos] : constRegistry.View<Position>())
    {
        (void)entity;
        seen.push_back(pos.get().x);
    }

    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen, (std::vector<float>{ 0.0f, 1.0f, 2.0f }));
}

TEST(RegistryTest, View_ConstRegistryReturnsReadOnlyReferences)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{ 1.0f, 1.0f }).IsOk());

    Registry const& constRegistry = registry;
    for (auto [entity, pos] : constRegistry.View<Position>())
    {
        (void)entity;
        static_assert(std::is_const_v<std::remove_reference_t<decltype(pos.get())>>,
            "Registry::View() const must yield read-only references");
        EXPECT_EQ(pos.get().x, 1.0f);
    }
}

TEST(RegistryTest, View_ConstRegistryEmptyForNeverUsedComponentType)
{
    Registry registry;
    auto e = registry.CreateEntity();
    ASSERT_TRUE(e.IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(e.Value(), Position{ 1.0f, 2.0f }).IsOk());

    // Velocity's pool was never created -- must stay empty through the const
    // overload exactly like the non-const one (View_NeverUsedComponentTypeIsEmpty).
    Registry const& constRegistry = registry;
    auto view = constRegistry.View<Position, Velocity>();

    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

TEST(RegistryTest, View_ConstRegistryOnlyVisitsEntitiesWithEveryRequestedComponent)
{
    Registry registry;
    auto both = registry.CreateEntity().Value();
    auto onlyPosition = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(both, Position{ 1.0f, 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Velocity>(both, Velocity{ 2.0f, 2.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(onlyPosition, Position{ 9.0f, 9.0f }).IsOk());

    Registry const& constRegistry = registry;
    std::vector<EntityIndex> seen;
    for (auto [entity, pos, vel] : constRegistry.View<Position, Velocity>())
    {
        (void)pos;
        (void)vel;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], both.m_Index);
}

// ─── Registry::View — Disable filtering ─────────────────────────────────────

TEST(RegistryTest, View_EntityWithDisable_IsSkippedByDefault)
{
    Registry registry;
    auto enabled = registry.CreateEntity().Value();
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(enabled, Position{ 1.0f, 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(disabled, Position{ 2.0f, 2.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : registry.View<Position>())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], enabled.m_Index);
}

TEST(RegistryTest, View_IncludeDisabled_EntityWithDisableIsVisible)
{
    Registry registry;
    auto enabled = registry.CreateEntity().Value();
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(enabled, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(disabled, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : registry.View<Position>().IncludeDisabled())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen, (std::vector<EntityIndex>{ enabled.m_Index, disabled.m_Index }));
}

TEST(RegistryTest, View_ExplicitlyRequestingDisable_IsNotFilteredOut)
{
    // Ts itself includes Disable here, so the "skip disabled entities"
    // behavior does not apply -- the caller is asking for disabled entities
    // by name, not accidentally filtering them out from under themselves.
    Registry registry;
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    auto view = registry.View<Disable>();
    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 1u);
}

TEST(RegistryTest, View_NoEntityEverTaggedDisabled_BehavesExactlyAsWithoutTheFeature)
{
    // Disable's pool is never created (FindPool<Disable>() is
    // nullptr) -- the view must still visit every entity normally rather
    // than mistaking "no disabled pool" for "everything filtered".
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{ 5.0f, 5.0f }).IsOk());

    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : registry.View<Position>())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], e.m_Index);
}

TEST(RegistryTest, View_ConstRegistry_EntityWithDisableIsSkippedByDefault)
{
    Registry registry;
    auto enabled = registry.CreateEntity().Value();
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(enabled, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Position>(disabled, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    Registry const& constRegistry = registry;
    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : constRegistry.View<Position>())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], enabled.m_Index);
}

TEST(RegistryTest, View_ConstRegistry_IncludeDisabledMakesItVisible)
{
    Registry registry;
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(disabled, Position{}).IsOk());
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    Registry const& constRegistry = registry;
    std::vector<EntityIndex> seen;
    for (auto [entity, pos] : constRegistry.View<Position>().IncludeDisabled())
    {
        (void)pos;
        seen.push_back(entity.m_Index);
    }

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], disabled.m_Index);
}

TEST(RegistryTest, View_ConstRegistry_ExplicitlyRequestingDisableIsNotFilteredOut)
{
    // Regression test: View<Ts const...>() used to compare the const-
    // qualified Ts against the unqualified Disable type, so this case
    // never matched and a const view over Disable itself came back
    // empty even when a disabled entity existed.
    Registry registry;
    auto disabled = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Disable>(disabled, Disable{}).IsOk());

    Registry const& constRegistry = registry;
    auto view = constRegistry.View<Disable>();
    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 1u);
}

// ─── Registry::DisableEntity / IsDisabled — hierarchy propagation ─────────────

TEST(RegistryTest, IsDisabled_EntityWithNoDisableAndNoHierarchy_IsFalse)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    EXPECT_FALSE(registry.IsDisabled(e));
}

TEST(RegistryTest, IsDisabled_DisableEntity_MarksItDisabled)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    registry.DisableEntity(e);
    EXPECT_TRUE(registry.IsDisabled(e));
}

TEST(RegistryTest, IsDisabled_ChildOfDisabledParent_IsDisabled)
{
    Registry registry;
    auto parent = registry.CreateEntity().Value();
    auto child = registry.CreateEntity().Value();
    AttachChild(registry, parent, child);

    registry.DisableEntity(parent);

    EXPECT_TRUE(registry.IsDisabled(child));
}

TEST(RegistryTest, IsDisabled_GrandchildOfDisabledAncestor_IsDisabled)
{
    // Regression case: IsDisabled walks the whole Hierarchy chain, not just
    // one level -- disabling a grandparent must reach a grandchild too.
    Registry registry;
    auto grandparent = registry.CreateEntity().Value();
    auto parent = registry.CreateEntity().Value();
    auto child = registry.CreateEntity().Value();
    AttachChild(registry, grandparent, parent);
    AttachChild(registry, parent, child);

    registry.DisableEntity(grandparent);

    EXPECT_TRUE(registry.IsDisabled(parent));
    EXPECT_TRUE(registry.IsDisabled(child));
}

TEST(RegistryTest, IsDisabled_AttachingUnderAnAlreadyDisabledParent_TheNewChildIsDisabled)
{
    Registry registry;
    auto parent = registry.CreateEntity().Value();
    registry.DisableEntity(parent);

    auto child = registry.CreateEntity().Value();
    AttachChild(registry, parent, child);

    EXPECT_TRUE(registry.IsDisabled(child));
}

TEST(RegistryTest, IsDisabled_DetachingFromADisabledParent_ChildIsNoLongerDisabled)
{
    Registry registry;
    auto parent = registry.CreateEntity().Value();
    auto child = registry.CreateEntity().Value();
    AttachChild(registry, parent, child);
    registry.DisableEntity(parent);
    ASSERT_TRUE(registry.IsDisabled(child));

    DetachChild(registry, child);

    EXPECT_FALSE(registry.IsDisabled(child));
}

TEST(RegistryTest, IsDisabled_RemovingDisableFromAnAncestor_WholeSubtreeIsNoLongerDisabled)
{
    // There is no separate "inherited disable" marker to forget to clean up
    // -- removing Disable from the ancestor is the only state that ever
    // needed changing, so the whole subtree is consistent immediately.
    Registry registry;
    auto parent = registry.CreateEntity().Value();
    auto child = registry.CreateEntity().Value();
    AttachChild(registry, parent, child);
    registry.DisableEntity(parent);
    ASSERT_TRUE(registry.IsDisabled(child));

    ASSERT_TRUE(registry.RemoveComponent<Disable>(parent).IsOk());

    EXPECT_FALSE(registry.IsDisabled(parent));
    EXPECT_FALSE(registry.IsDisabled(child));
}

TEST(RegistryTest, View_ChildOfDisabledParent_IsSkippedEvenThoughOnlyTheParentCarriesDisable)
{
    Registry registry;
    auto parent = registry.CreateEntity().Value();
    auto child = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(child, Position{}).IsOk());
    AttachChild(registry, parent, child);

    registry.DisableEntity(parent);

    auto view = registry.View<Position>();
    std::size_t count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) ++count;
    EXPECT_EQ(count, 0u);
}

// ─── Registry::GetComponent / HasComponent — const and non-const access ───────

TEST(RegistryTest, GetComponent_MutableRegistryReturnsMutableReference)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{ 1.0f, 1.0f }).IsOk());

    auto result = registry.GetComponent<Position>(e);
    ASSERT_TRUE(result.IsOk());
    result.Value().get().x = 42.0f; // only compiles if the overload returns a mutable reference

    EXPECT_EQ(registry.GetComponent<Position>(e).Value().get().x, 42.0f);
}

TEST(RegistryTest, GetComponent_ConstRegistryReturnsStoredValue)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(e, Position{ 3.0f, 4.0f }).IsOk());

    Registry const& constRegistry = registry;
    auto result = constRegistry.GetComponent<Position>(e);
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get(), (Position{ 3.0f, 4.0f }));
}

TEST(RegistryTest, GetComponent_ConstRegistryUnknownComponentTypeReturnsError)
{
    Registry registry;
    auto e = registry.CreateEntity().Value();
    // Position's pool was never created — no entity has ever had one.

    Registry const& constRegistry = registry;
    auto result = constRegistry.GetComponent<Position>(e);
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::InvalidComponent));
}

TEST(RegistryTest, GetComponent_ConstRegistryEntityWithoutComponentReturnsError)
{
    Registry registry;
    auto withPosition = registry.CreateEntity().Value();
    auto without = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(withPosition, Position{}).IsOk());

    // Position's pool exists (withPosition uses it), but without was never
    // inserted into it — a different error than "type never used at all".
    Registry const& constRegistry = registry;
    auto result = constRegistry.GetComponent<Position>(without);
    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::EntityNotAttachedToComponent));
}

TEST(RegistryTest, HasComponent_ConstRegistryReflectsPresenceAndAbsence)
{
    Registry registry;
    auto withPosition = registry.CreateEntity().Value();
    auto without = registry.CreateEntity().Value();
    ASSERT_TRUE(registry.AddComponent<Position>(withPosition, Position{}).IsOk());

    Registry const& constRegistry = registry;
    EXPECT_TRUE(constRegistry.HasComponent<Position>(withPosition));
    EXPECT_FALSE(constRegistry.HasComponent<Position>(without));
    EXPECT_FALSE(constRegistry.HasComponent<Velocity>(withPosition)); // Velocity's pool was never created
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

// ─── Registry::SetResource / GetResource ───────────────────────────────────────

TEST(RegistryTest, GetResource_NeverSetReturnsError)
{
    Registry registry;

    auto result = registry.GetResource<GameSettings>();

    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::ResourceNotSet));
}

TEST(RegistryTest, SetResource_ThenGetResourceReturnsIt)
{
    Registry registry;
    registry.SetResource(GameSettings{ .m_MaxPlayers = 8 });

    auto result = registry.GetResource<GameSettings>();

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get(), (GameSettings{ .m_MaxPlayers = 8 }));
}

TEST(RegistryTest, SetResource_CalledAgainReplacesThePreviousValue)
{
    Registry registry;
    registry.SetResource(GameSettings{ .m_MaxPlayers = 8 });
    registry.SetResource(GameSettings{ .m_MaxPlayers = 2 });

    auto result = registry.GetResource<GameSettings>();

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().get().m_MaxPlayers, 2);
}

TEST(RegistryTest, GetResource_ReturnsAMutableReferenceIntoTheStoredValue)
{
    Registry registry;
    registry.SetResource(GameSettings{ .m_MaxPlayers = 4 });

    registry.GetResource<GameSettings>().Value().get().m_MaxPlayers = 16;

    EXPECT_EQ(registry.GetResource<GameSettings>().Value().get().m_MaxPlayers, 16);
}

TEST(RegistryTest, SetResource_DifferentTypesAreIndependent)
{
    Registry registry;
    registry.SetResource(GameSettings{ .m_MaxPlayers = 8 });
    registry.SetResource(GameSpeed{ .m_Multiplier = 2.0f });

    EXPECT_EQ(registry.GetResource<GameSettings>().Value().get().m_MaxPlayers, 8);
    EXPECT_FLOAT_EQ(registry.GetResource<GameSpeed>().Value().get().m_Multiplier, 2.0f);
}

TEST(RegistryTest, GetResource_UnsetOnOneRegistryIsUnaffectedByAnotherRegistrySettingIt)
{
    // Resource ids are assigned from a counter shared across every Registry
    // instance, but storage (m_Resources) is per-instance -- setting T on
    // one registry must not make a *different*, fresh registry believe T is
    // set too just because T already has an id by the time it runs.
    Registry withSetting;
    withSetting.SetResource(GameSettings{ .m_MaxPlayers = 8 });

    Registry withoutSetting;
    auto result = withoutSetting.GetResource<GameSettings>();

    EXPECT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::EcsError::ResourceNotSet));
}

}
