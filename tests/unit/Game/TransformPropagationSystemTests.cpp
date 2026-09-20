#include <ASGE/Game/Systems/TransformPropagationSystem.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace
{

using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::game::components::AttachChild;
using asge::game::components::Hierarchy;
using asge::game::components::Transform;
using asge::math::Float2;

constexpr float kPi = 3.14159265358979323846f;

Entity MakeRoot(Registry& inRegistry, Transform inTransform)
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), inTransform).IsOk());
    return entity.Value();
}

// ─── Root entities (no Hierarchy, or one with no parent) ────────────────────

TEST(TransformPropagationSystemTest, DirtyRootWithNoHierarchy_CopiesLocalIntoWorldAndClearsDirty)
{
    Registry registry;
    Transform t{};
    t.m_LocalCoordinates = {10.0f, 20.0f};
    t.m_LocalScale = {2.0f, 3.0f};
    t.m_LocalRotation = 1.5f;
    t.m_Dirty = true;
    auto entity = MakeRoot(registry, t);

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(entity).Value().get();
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.x(), 10.0f);
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.y(), 20.0f);
    EXPECT_FLOAT_EQ(result.m_WorldScale.x(), 2.0f);
    EXPECT_FLOAT_EQ(result.m_WorldScale.y(), 3.0f);
    EXPECT_FLOAT_EQ(result.m_WorldRotation, 1.5f);
    EXPECT_FALSE(result.m_Dirty);
}

TEST(TransformPropagationSystemTest, NotDirtyRoot_LeavesWorldUntouched)
{
    Registry registry;
    Transform t{};
    t.m_LocalCoordinates = {10.0f, 20.0f};
    t.m_WorldCoordinates = {999.0f, 999.0f}; // deliberately mismatched from Local
    t.m_Dirty = false;
    auto entity = MakeRoot(registry, t);

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(entity).Value().get();
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.x(), 999.0f);
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.y(), 999.0f);
}

TEST(TransformPropagationSystemTest, DirtyRootWithHierarchyButNoParent_StillTreatedAsARoot)
{
    Registry registry;
    Transform t{};
    t.m_LocalCoordinates = {5.0f, 6.0f};
    t.m_Dirty = true;
    auto entity = MakeRoot(registry, t);
    registry.AddComponent(entity, Hierarchy{}); // m_Parent defaults to Entity::Null()

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(entity).Value().get();
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.x(), 5.0f);
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.y(), 6.0f);
    EXPECT_FALSE(result.m_Dirty);
}

// ─── Parent/child composition ────────────────────────────────────────────────

TEST(TransformPropagationSystemTest, DirtyParent_PropagatesPositionToItsChild)
{
    Registry registry;

    Transform parentT{};
    parentT.m_LocalCoordinates = {100.0f, 50.0f};
    parentT.m_Dirty = true;
    auto parent = MakeRoot(registry, parentT);

    Transform childT{};
    childT.m_LocalCoordinates = {10.0f, 0.0f};
    auto child = MakeRoot(registry, childT);

    AttachChild(registry, parent, child);

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(child).Value().get();
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.x(), 110.0f); // parent world (100,50) + child local (10,0)
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.y(), 50.0f);
}

TEST(TransformPropagationSystemTest, ParentScaleAndRotation_ComposeIntoTheChildsWorldTransform)
{
    Registry registry;

    Transform parentT{};
    parentT.m_LocalScale = {2.0f, 2.0f};
    parentT.m_LocalRotation = kPi * 0.5f; // 90 degrees
    parentT.m_Dirty = true;
    auto parent = MakeRoot(registry, parentT);

    Transform childT{};
    childT.m_LocalCoordinates = {1.0f, 0.0f};
    childT.m_LocalScale = {3.0f, 3.0f};
    childT.m_LocalRotation = 0.25f;
    auto child = MakeRoot(registry, childT);

    AttachChild(registry, parent, child);

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(child).Value().get();
    // Child's local (1,0) is scaled by the parent's world scale (2,2) -> (2,0),
    // then rotated by the parent's world rotation (90 degrees) -> ~(0,2),
    // then offset by the parent's world position (0,0).
    EXPECT_NEAR(result.m_WorldCoordinates.x(), 0.0f, 1e-4f);
    EXPECT_NEAR(result.m_WorldCoordinates.y(), 2.0f, 1e-4f);
    EXPECT_FLOAT_EQ(result.m_WorldScale.x(), 6.0f); // 2 * 3
    EXPECT_FLOAT_EQ(result.m_WorldScale.y(), 6.0f);
    EXPECT_NEAR(result.m_WorldRotation, kPi * 0.5f + 0.25f, 1e-4f);
}

TEST(TransformPropagationSystemTest, NotDirtyParent_StaticParent_DirtyChildStillPropagatesOnItsOwn)
{
    Registry registry;

    Transform parentT{};
    parentT.m_LocalCoordinates = {100.0f, 0.0f};
    parentT.m_WorldCoordinates = {100.0f, 0.0f};
    parentT.m_Dirty = false; // already propagated in an earlier call
    auto parent = MakeRoot(registry, parentT);

    Transform childT{};
    childT.m_LocalCoordinates = {5.0f, 0.0f};
    childT.m_Dirty = true; // only the child moved this frame
    auto child = MakeRoot(registry, childT);

    AttachChild(registry, parent, child);
    // AttachChild already marks the child dirty; explicit here for clarity
    // regardless of that side effect.
    registry.GetComponent<Transform>(child).Value().get().m_Dirty = true;

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& parentResult = registry.GetComponent<Transform>(parent).Value().get();
    auto const& childResult = registry.GetComponent<Transform>(child).Value().get();
    EXPECT_FLOAT_EQ(parentResult.m_WorldCoordinates.x(), 100.0f); // untouched -- parent wasn't dirty
    EXPECT_FLOAT_EQ(childResult.m_WorldCoordinates.x(), 105.0f);  // still recomposed from the parent's world
}

TEST(TransformPropagationSystemTest, NeitherParentNorChildDirty_NeitherWorldTransformChanges)
{
    Registry registry;

    Transform parentT{};
    parentT.m_LocalCoordinates = {100.0f, 0.0f};
    parentT.m_WorldCoordinates = {100.0f, 0.0f};
    parentT.m_Dirty = false;
    auto parent = MakeRoot(registry, parentT);

    Transform childT{};
    childT.m_LocalCoordinates = {5.0f, 0.0f};
    childT.m_WorldCoordinates = {105.0f, 0.0f};
    childT.m_Dirty = false;
    auto child = MakeRoot(registry, childT);

    AttachChild(registry, parent, child);
    // Undo AttachChild's own dirty-marking so this test genuinely exercises
    // the "nothing moved" path.
    registry.GetComponent<Transform>(child).Value().get().m_Dirty = false;

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& childResult = registry.GetComponent<Transform>(child).Value().get();
    EXPECT_FLOAT_EQ(childResult.m_WorldCoordinates.x(), 105.0f);
}

TEST(TransformPropagationSystemTest, PropagatedChild_ActuallyWritesBackToTheRegistryNotAThrowawayCopy)
{
    // Regression guard: PropagateToChildren must bind the child's Transform
    // by reference; copying it would let ApplyPropagation "succeed" against
    // a temporary while the registry's own component stays stale.
    Registry registry;

    Transform parentT{};
    parentT.m_LocalCoordinates = {50.0f, 0.0f};
    parentT.m_Dirty = true;
    auto parent = MakeRoot(registry, parentT);

    Transform childT{};
    childT.m_LocalCoordinates = {1.0f, 0.0f};
    auto child = MakeRoot(registry, childT);
    AttachChild(registry, parent, child);

    asge::game::systems::TransformPropagationSystem(registry);

    // Re-fetching from the registry (rather than reusing any reference held
    // across the call) to make sure the persisted component itself changed.
    auto const& persisted = registry.GetComponent<Transform>(child).Value().get();
    EXPECT_FLOAT_EQ(persisted.m_WorldCoordinates.x(), 51.0f);
    EXPECT_FALSE(persisted.m_Dirty);
}

TEST(TransformPropagationSystemTest, Grandchild_ComposesTransitivelyThroughTheWholeChain)
{
    Registry registry;

    Transform grandparentT{};
    grandparentT.m_LocalCoordinates = {10.0f, 0.0f};
    grandparentT.m_Dirty = true;
    auto grandparent = MakeRoot(registry, grandparentT);

    Transform parentT{};
    parentT.m_LocalCoordinates = {10.0f, 0.0f};
    auto parent = MakeRoot(registry, parentT);
    AttachChild(registry, grandparent, parent);

    Transform childT{};
    childT.m_LocalCoordinates = {10.0f, 0.0f};
    auto child = MakeRoot(registry, childT);
    AttachChild(registry, parent, child);

    asge::game::systems::TransformPropagationSystem(registry);

    EXPECT_FLOAT_EQ(
        registry.GetComponent<Transform>(parent).Value().get().m_WorldCoordinates.x(), 20.0f);
    EXPECT_FLOAT_EQ(
        registry.GetComponent<Transform>(child).Value().get().m_WorldCoordinates.x(), 30.0f);
}

TEST(TransformPropagationSystemTest, IntermediateNodeWithNoTransform_BlocksPropagationIntoItsOwnChildren)
{
    // A Hierarchy node with no Transform of its own has no world transform
    // to compose its children against, so its whole subtree is skipped --
    // this is current, deliberate behavior, not something this test expects
    // to change.
    Registry registry;

    Transform rootT{};
    rootT.m_LocalCoordinates = {10.0f, 0.0f};
    rootT.m_Dirty = true;
    auto root = MakeRoot(registry, rootT);

    auto transformlessMiddle = registry.CreateEntity();
    ASSERT_TRUE(transformlessMiddle.IsOk());
    AttachChild(registry, root, transformlessMiddle.Value());

    Transform grandchildT{};
    grandchildT.m_LocalCoordinates = {5.0f, 0.0f};
    grandchildT.m_WorldCoordinates = {123.0f, 456.0f}; // sentinel: must survive untouched
    auto grandchild = MakeRoot(registry, grandchildT);
    AttachChild(registry, transformlessMiddle.Value(), grandchild);

    asge::game::systems::TransformPropagationSystem(registry);

    auto const& result = registry.GetComponent<Transform>(grandchild).Value().get();
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.x(), 123.0f);
    EXPECT_FLOAT_EQ(result.m_WorldCoordinates.y(), 456.0f);
}

TEST(TransformPropagationSystemTest, MultipleChildrenOfOneParent_AllGetPropagated)
{
    Registry registry;

    Transform parentT{};
    parentT.m_LocalCoordinates = {100.0f, 0.0f};
    parentT.m_Dirty = true;
    auto parent = MakeRoot(registry, parentT);

    Transform child1T{};
    child1T.m_LocalCoordinates = {1.0f, 0.0f};
    auto child1 = MakeRoot(registry, child1T);
    AttachChild(registry, parent, child1);

    Transform child2T{};
    child2T.m_LocalCoordinates = {2.0f, 0.0f};
    auto child2 = MakeRoot(registry, child2T);
    AttachChild(registry, parent, child2);

    asge::game::systems::TransformPropagationSystem(registry);

    EXPECT_FLOAT_EQ(
        registry.GetComponent<Transform>(child1).Value().get().m_WorldCoordinates.x(), 101.0f);
    EXPECT_FLOAT_EQ(
        registry.GetComponent<Transform>(child2).Value().get().m_WorldCoordinates.x(), 102.0f);
}

}
