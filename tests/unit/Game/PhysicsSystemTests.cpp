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
using asge::game::systems::PhysicsState;

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

// The collision-only slice of PhysicsUpdate (no GravitySystem/MovementSystem)
// -- detect, resolve, dispatch trigger events, in the same order PhysicsUpdate
// runs them, for tests that want deterministic collision behavior on its own.
void RunCollisionResolution(Registry& inRegistry, PhysicsState& inState)
{
    auto contacts = asge::game::systems::DetectCollisions(inRegistry);
    asge::game::systems::ResolveCollisions(inRegistry, contacts);
    asge::game::systems::DispatchTriggerEvents(inState, contacts);
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

    PhysicsState state;
    RunCollisionResolution(registry, state);

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

    PhysicsState state;
    RunCollisionResolution(registry, state);

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

    PhysicsState state;
    RunCollisionResolution(registry, state);

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

    PhysicsState state;
    RunCollisionResolution(registry, state);

    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e1).Value().get().m_X, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(e2).Value().get().m_X, 6.0f);
}

// ─── CollisionResolution — incomplete entities ──────────────────────────────

TEST(PhysicsSystemTest, EntityMissingTransformOrCollider_SkippedNotCrashed)
{
    Registry registry;

    // Complete: participates in the view. The other two are each missing
    // one of the two components the view requires, so View<Transform,
    // Collider> must skip them rather than DetectCollisions crashing.
    auto complete = MakeCollider(registry, 0.0f, 0.0f, 10.0f, 10.0f);

    auto colliderOnly = registry.CreateEntity();
    ASSERT_TRUE(colliderOnly.IsOk());
    ASSERT_TRUE(registry.AddComponent(colliderOnly.Value(),
        Collider{ .m_LocalBounds = asge::math::Rect{ 0.0f, 0.0f, 10.0f, 10.0f } }).IsOk());

    auto transformOnly = registry.CreateEntity();
    ASSERT_TRUE(transformOnly.IsOk());
    ASSERT_TRUE(registry.AddComponent(transformOnly.Value(), Transform{ .m_X = 5.0f }).IsOk());

    PhysicsState state;
    EXPECT_NO_THROW(RunCollisionResolution(registry, state));

    // Only one entity actually qualifies for the view, so nothing overlaps it.
    EXPECT_FLOAT_EQ(registry.GetComponent<Transform>(complete).Value().get().m_X, 0.0f);
}

// ─── CollisionResolution — Trigger colliders ────────────────────────────────

// events::OnCollisionTriggerEnter/Stay/Exit are process-wide signals (not
// per-Registry), so every test here must disconnect its own listeners in
// TearDown -- a listener left connected past its test would still fire
// (against a dangling capture) the next time any test runs collision
// resolution. m_Overlaps only tracks Enter, matching the old single-signal
// OnTriggerOverlap tests below (each calls RunCollisionResolution once, so
// a first-time overlap is exactly what Enter reports); the Stay/Exit tests
// further down use their own local connections instead.
class TriggerCollisionTest : public ::testing::Test
{
protected:
    Registry m_Registry;
    PhysicsState m_PhysicsState;
    std::vector<std::pair<Entity, Entity>> m_Overlaps;
    asge::signals::Connection<Entity, Entity> m_Connection;

    void SetUp() override
    {
        m_Connection = asge::game::events::OnCollisionTriggerEnter().Connect(
            [this]( Entity inA, Entity inB ) { m_Overlaps.emplace_back( inA, inB ); }
        );
    }

    void TearDown() override
    {
        m_Connection.Disconnect();
    }

    void RunCollisions() { RunCollisionResolution( m_Registry, m_PhysicsState ); }
};

TEST_F(TriggerCollisionTest, TwoOverlappingTriggers_FiresOnceAndAppliesNoPushOut)
{
    // Both movable (Velocity + Rigidbody) so a push-out would be visible if
    // ResolveCollisions mistakenly applied one to a Trigger pair.
    auto e1 = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto e2 = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    ASSERT_TRUE(m_Registry.AddComponent(e1, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e2, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e1, Rigidbody{}).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(e2, Rigidbody{}).IsOk());

    RunCollisions();

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

    RunCollisions();

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

    RunCollisions();

    ASSERT_EQ(m_Overlaps.size(), 1u);
    EXPECT_EQ(m_Overlaps[0].first, trigger);
    EXPECT_EQ(m_Overlaps[0].second, other);
}

TEST_F(TriggerCollisionTest, NonOverlappingTriggerAndSolid_DoesNotFire)
{
    MakeCollider(m_Registry, 0.0f, 0.0f, 5.0f, 5.0f, ResolutionType::Trigger);
    MakeCollider(m_Registry, 50.0f, 0.0f, 5.0f, 5.0f);

    RunCollisions();

    EXPECT_TRUE(m_Overlaps.empty());
}

TEST_F(TriggerCollisionTest, UnknownResolutionOnEitherSide_SkipsBothPushOutAndTrigger)
{
    // Unknown is reachable in practice from a Collider deserialized with an
    // unrecognized m_Resolution value -- DetectCollisions must ignore
    // the pair entirely rather than guessing Solid or Trigger.
    auto unknown = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Unknown);
    auto movable = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f);
    ASSERT_TRUE(m_Registry.AddComponent(movable, Velocity{ .m_DX = 5.0f }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent(movable, Rigidbody{}).IsOk());

    RunCollisions();

    EXPECT_TRUE(m_Overlaps.empty());
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(movable).Value().get().m_X, 6.0f); // untouched
    EXPECT_FLOAT_EQ(m_Registry.GetComponent<Transform>(unknown).Value().get().m_X, 0.0f);
}

// ─── DispatchTriggerEvents — Enter/Stay/Exit across multiple frames ─────────

TEST_F(TriggerCollisionTest, StillOverlappingNextCall_FiresStayNotAnotherEnter)
{
    std::vector<std::pair<Entity, Entity>> stays;
    auto stayConnection = asge::game::events::OnCollisionTriggerStay().Connect(
        [&stays]( Entity inA, Entity inB ) { stays.emplace_back( inA, inB ); }
    );

    auto e1 = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto e2 = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);

    RunCollisions(); // first call, new overlap -> Enter
    RunCollisions(); // still overlapping, same PhysicsState -> Stay, not another Enter

    stayConnection.Disconnect();

    EXPECT_EQ(m_Overlaps.size(), 1u); // Enter fired exactly once, on the first call
    ASSERT_EQ(stays.size(), 1u);      // Stay fired exactly once, on the second call
    EXPECT_EQ(stays[0].first, e1);
    EXPECT_EQ(stays[0].second, e2);
}

TEST_F(TriggerCollisionTest, NoLongerOverlappingNextCall_FiresExitOnce)
{
    std::vector<std::pair<Entity, Entity>> exits;
    auto exitConnection = asge::game::events::OnCollisionTriggerExit().Connect(
        [&exits]( Entity inA, Entity inB ) { exits.emplace_back( inA, inB ); }
    );

    auto e1 = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto e2 = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);

    RunCollisions(); // first call, overlapping -> Enter

    // Move e2 out of range so the pair no longer overlaps on the next call.
    m_Registry.GetComponent<Transform>(e2).Value().get().m_X = 500.0f;
    RunCollisions(); // no longer overlapping, same PhysicsState -> Exit

    exitConnection.Disconnect();

    EXPECT_EQ(m_Overlaps.size(), 1u); // Enter fired once, on the first call only
    ASSERT_EQ(exits.size(), 1u);
    EXPECT_EQ(exits[0].first, e1);
    EXPECT_EQ(exits[0].second, e2);
}

TEST_F(TriggerCollisionTest, DestroyedOverlappingEntity_StillFiresExit)
{
    // DispatchTriggerEvents' doc comment calls this out explicitly: a pair
    // missing from this frame's contacts because one side no longer exists
    // must still resolve to Exit, the same as if it had simply moved away.
    std::vector<std::pair<Entity, Entity>> exits;
    auto exitConnection = asge::game::events::OnCollisionTriggerExit().Connect(
        [&exits]( Entity inA, Entity inB ) { exits.emplace_back( inA, inB ); }
    );

    auto e1 = MakeCollider(m_Registry, 0.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);
    auto e2 = MakeCollider(m_Registry, 6.0f, 0.0f, 10.0f, 10.0f, ResolutionType::Trigger);

    RunCollisions(); // Enter
    ASSERT_TRUE(m_Registry.DestroyEntity(e2).IsOk());
    RunCollisions(); // e2 no longer exists -> absent from this frame's contacts -> Exit

    exitConnection.Disconnect();

    ASSERT_EQ(exits.size(), 1u);
    EXPECT_EQ(exits[0].first, e1);
    EXPECT_EQ(exits[0].second, e2);
}

}
