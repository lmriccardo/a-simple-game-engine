#include <ASGE/Game/Systems/PhysicsSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Velocity.hpp>
#include <ASGE/Game/Components/Collider.hpp>
#include <ASGE/Game/Components/Rigidbody.hpp>
#include <ASGE/Game/Events.hpp>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::game::components::Collider;
using asge::game::components::ResolutionType;
using asge::game::components::Rigidbody;
using asge::game::components::Transform;
using asge::game::components::Velocity;

Entity MakeCollider(Registry& inRegistry, float inX, float inY, float inW, float inH,
    ResolutionType inResolution = ResolutionType::Solid)
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Transform{ .m_X = inX, .m_Y = inY }).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Collider{
        .m_LocalBounds = asge::math::Rect{ 0.0f, 0.0f, inW, inH },
        .m_Resolution = inResolution
    }).IsOk());
    return entity.Value();
}

// ─── CollisionResolution — both entities movable ────────────────────────────

TEST(PhysicsSystemTest, TwoOverlappingMovableEntities_PushedApartEvenlyOnLeastPenetrationAxis)
{
    Registry registry;

    // Two 10x10 boxes overlapping by 4 on X, fully overlapping on Y (10) --
    // X is the axis of least penetration, so resolution happens along X.
    auto e1 = MakeCollider(registry, 0.0f, 0.0f, 10.0f, 10.0f);
    auto e2 = MakeCollider(registry, 6.0f, 0.0f, 10.0f, 10.0f);
    ASSERT_TRUE(registry.AddComponent(e1, Velocity{ .m_DX = 5.0f, .m_DY = 3.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(e2, Velocity{ .m_DX = 5.0f, .m_DY = 3.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(e1, Rigidbody{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(e2, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(registry);

    // Equal masses (Rigidbody{}'s default), so the total correction (4) is
    // split evenly, pushing each entity away from the other.
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e1).Value().get().m_X, -2.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e2).Value().get().m_X, 8.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e1).Value().get().m_Y, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e2).Value().get().m_Y, 0.0f);

    // Corrected axis (X) is zeroed on both; the untouched axis (Y) is left alone.
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e1).Value().get().m_DX, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e2).Value().get().m_DX, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e1).Value().get().m_DY, 3.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e2).Value().get().m_DY, 3.0f);
}

// ─── CollisionResolution — one entity static (no Velocity/Rigidbody) ────────

TEST(PhysicsSystemTest, MovableOverlappingStaticEntity_OnlyMovableGetsTheFullCorrection)
{
    Registry registry;

    auto movable = MakeCollider(registry, 0.0f, 0.0f, 10.0f, 10.0f);
    auto immovable = MakeCollider(registry, 6.0f, 0.0f, 10.0f, 10.0f); // e.g. static level geometry
    ASSERT_TRUE(registry.AddComponent(movable, Velocity{ .m_DX = 5.0f, .m_DY = 3.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(movable, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(registry);

    // The other side has neither a Velocity nor a Rigidbody, so the movable
    // entity absorbs the whole correction -- and the immovable one's
    // Transform is never touched.
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(movable).Value().get().m_X, -4.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(immovable).Value().get().m_X, 6.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(movable).Value().get().m_DX, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(movable).Value().get().m_DY, 3.0f);
}

// ─── CollisionResolution — no overlap / no movable entities ─────────────────

TEST(PhysicsSystemTest, TwoMovableNonOverlappingEntities_BothLeftUntouched)
{
    Registry registry;

    auto e1 = MakeCollider(registry, 0.0f, 0.0f, 5.0f, 5.0f);
    auto e2 = MakeCollider(registry, 20.0f, 0.0f, 5.0f, 5.0f);
    ASSERT_TRUE(registry.AddComponent(e1, Velocity{ .m_DX = 1.0f, .m_DY = 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(e2, Velocity{ .m_DX = 2.0f, .m_DY = 2.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(e1, Rigidbody{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(e2, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(registry);

    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e1).Value().get().m_X, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e2).Value().get().m_X, 20.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e1).Value().get().m_DX, 1.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Velocity>(e2).Value().get().m_DX, 2.0f);
}

TEST(PhysicsSystemTest, TwoOverlappingImmovableEntities_BothLeftUntouched)
{
    Registry registry;

    // Neither has a Velocity -- nothing is movable, so there's nothing to push.
    auto e1 = MakeCollider(registry, 0.0f, 0.0f, 10.0f, 10.0f);
    auto e2 = MakeCollider(registry, 6.0f, 0.0f, 10.0f, 10.0f);

    asge::game::systems::CollisionResolution(registry);

    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e1).Value().get().m_X, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e2).Value().get().m_X, 6.0f);
}

// ─── CollisionResolution — incomplete entities ──────────────────────────────

TEST(PhysicsSystemTest, EntityMissingTransformOrCollider_SkippedNotCrashed)
{
    Registry registry;

    // Complete: participates in the view. The other two are each missing
    // one of the two components the view requires, so View<Transform,
    // Collider> must skip them rather than CollisionResolution crashing.
    auto complete = MakeCollider(registry, 0.0f, 0.0f, 10.0f, 10.0f);

    auto colliderOnly = registry.CreateEntity();
    ASSERT_TRUE(colliderOnly.IsOk());
    ASSERT_TRUE(registry.AddComponent(colliderOnly.Value(),
        Collider{ .m_LocalBounds = asge::math::Rect{ 0.0f, 0.0f, 10.0f, 10.0f } }).IsOk());

    auto transformOnly = registry.CreateEntity();
    ASSERT_TRUE(transformOnly.IsOk());
    ASSERT_TRUE(registry.AddComponent(transformOnly.Value(), Transform{ .m_X = 5.0f }).IsOk());

    EXPECT_NO_THROW(asge::game::systems::CollisionResolution(registry));

    // Only one entity actually qualifies for the view, so nothing overlaps it.
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(complete).Value().get().m_X, 0.0f);
}

// ─── CollisionResolution — Trigger colliders ────────────────────────────────

// events::OnTriggerOverlap() is one process-wide signal (not per-Registry),
// so every test here must disconnect its own listener in TearDown -- a
// listener left connected past its test would still fire (against a
// dangling capture) the next time any test calls CollisionResolution.
class TriggerCollisionTest : public ::testing::Test
{
protected:
    Registry m_Registry;
    std::vector<std::pair<Entity, Entity>> m_Overlaps;
    asge::signals::Connection<Entity, Entity> m_Connection;

    void SetUp() override
    {
        m_Connection = asge::game::events::OnTriggerOverlap().Connect(
            [this]( Entity inA, Entity inB ) { m_Overlaps.emplace_back( inA, inB ); }
        );
    }

    void TearDown() override
    {
        m_Connection.Disconnect();
    }
};

TEST_F(TriggerCollisionTest, TwoOverlappingTriggers_FiresOnceAndAppliesNoPushOut)
{
    // Both movable (Velocity + Rigidbody) so a push-out would be visible if
    // CollisionResolution mistakenly applied one to a Trigger pair.
    auto e1 = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto e2 = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    ASSERT_TRUE(m_Registry.AddComponent(e1, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e2, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e1, Rigidbody{}).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e2, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(m_Registry);

    ASSERT_EQ(m_Overlaps.size(), 1u);
    EXPECT_EQ(m_Overlaps[0].first, e1);
    EXPECT_EQ(m_Overlaps[0].second, e2);

    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(e1).Value().get().m_X, 0.0f);
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(e2).Value().get().m_X, 6.0f);
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Velocity>(e1).Value().get().m_DX, 5.0f); // not zeroed
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Velocity>(e2).Value().get().m_DX, 5.0f);
}

TEST_F(TriggerCollisionTest, TriggerOverlappingMovableSolid_FiresTriggerFirstAndSkipsPushOutOnBothSides)
{
    auto solid = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f); // default: Solid
    auto trigger = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    ASSERT_TRUE(m_Registry.AddComponent(solid, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(solid, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(m_Registry);

    ASSERT_EQ(m_Overlaps.size(), 1u);
    EXPECT_EQ(m_Overlaps[0].first, trigger);  // the Trigger is always reported first ...
    EXPECT_EQ(m_Overlaps[0].second, solid);   // ... regardless of View's own pair order (solid was created first)

    // The Solid side is movable, but a Trigger pair never pushes anyone out.
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(solid).Value().get().m_X, 0.0f);
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Velocity>(solid).Value().get().m_DX, 5.0f);
}

TEST_F(TriggerCollisionTest, TriggerOverlappingStaticCollider_StillFires)
{
    // Neither side has a Velocity/Rigidbody -- confirms the trigger event
    // doesn't depend on either side being "movable" the way Solid does.
    auto trigger = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto other = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f);

    asge::game::systems::CollisionResolution(m_Registry);

    ASSERT_EQ(m_Overlaps.size(), 1u);
    EXPECT_EQ(m_Overlaps[0].first, trigger);
    EXPECT_EQ(m_Overlaps[0].second, other);
}

TEST_F(TriggerCollisionTest, NonOverlappingTriggerAndSolid_DoesNotFire)
{
    MakeCollider(m_Registry, 0.0f, 0.0f, 5.0f, 5.0f, ResolutionType::Trigger);
    MakeCollider(m_Registry, 50.0f, 0.0f, 5.0f, 5.0f);

    asge::game::systems::CollisionResolution(m_Registry);

    EXPECT_TRUE(m_Overlaps.empty());
}

TEST_F(TriggerCollisionTest, UnknownResolutionOnEitherSide_SkipsBothPushOutAndTrigger)
{
    // Unknown is reachable in practice from a Collider deserialized with an
    // unrecognized m_Resolution value -- CollisionResolution must ignore
    // the pair entirely rather than guessing Solid or Trigger.
    auto unknown = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Unknown);
    auto movable = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f);
    ASSERT_TRUE(m_Registry.AddComponent(movable, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(movable, Rigidbody{}).IsOk());

    asge::game::systems::CollisionResolution(m_Registry);

    EXPECT_TRUE(m_Overlaps.empty());
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(movable).Value().get().m_X, 6.0f); // untouched
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(unknown).Value().get().m_X, 0.0f);
}

}
