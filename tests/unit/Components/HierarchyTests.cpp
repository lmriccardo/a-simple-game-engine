#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::game::components::AttachChild;
using asge::game::components::DetachChild;
using asge::ecs::components::DestroyEntityGraph;
using asge::ecs::components::ForEachChild;
using asge::ecs::components::Hierarchy;
using asge::ecs::components::IsAncestor;
using asge::ecs::components::SanitizeHierarchy;
using asge::game::components::Transform;

Entity MakeEntity(Registry& inRegistry)
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    return entity.Value();
}

// ─── AttachChild ──────────────────────────────────────────────────────────────

TEST(AttachChildTest, FirstChild_LinksParentAndChildBothWays)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);

    AttachChild(registry, parent, child);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& childH = registry.GetComponent<Hierarchy>(child).Value().get();
    EXPECT_EQ(parentH.m_FirstChild, child);
    EXPECT_EQ(parentH.m_LastChild, child);
    EXPECT_EQ(childH.m_Parent, parent);
    EXPECT_EQ(childH.m_PrevSibling, Entity::Null());
    EXPECT_EQ(childH.m_NextSibling, Entity::Null());
}

TEST(AttachChildTest, SecondChild_AppendedAfterTheFirstAsTheNewLastChild)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto first = MakeEntity(registry);
    auto second = MakeEntity(registry);

    AttachChild(registry, parent, first);
    AttachChild(registry, parent, second);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& firstH = registry.GetComponent<Hierarchy>(first).Value().get();
    auto const& secondH = registry.GetComponent<Hierarchy>(second).Value().get();

    EXPECT_EQ(parentH.m_FirstChild, first);
    EXPECT_EQ(parentH.m_LastChild, second);
    EXPECT_EQ(firstH.m_NextSibling, second);
    EXPECT_EQ(secondH.m_PrevSibling, first);
    EXPECT_EQ(secondH.m_NextSibling, Entity::Null());
}

TEST(AttachChildTest, SelfParenting_IsANoOp)
{
    Registry registry;
    auto entity = MakeEntity(registry);

    AttachChild(registry, entity, entity);

    EXPECT_FALSE(registry.HasComponent<Hierarchy>(entity));
}

TEST(AttachChildTest, AttachingAnAncestorAsItsOwnDescendantsChild_IsRejected)
{
    Registry registry;
    auto grandparent = MakeEntity(registry);
    auto parent = MakeEntity(registry);
    AttachChild(registry, grandparent, parent);

    // Would create a cycle: grandparent is already parent's ancestor.
    AttachChild(registry, parent, grandparent);

    auto const& grandparentH = registry.GetComponent<Hierarchy>(grandparent).Value().get();
    EXPECT_EQ(grandparentH.m_Parent, Entity::Null()); // unchanged
    EXPECT_EQ(grandparentH.m_FirstChild, parent);      // unchanged
}

TEST(AttachChildTest, AlreadyAttachedElsewhere_DetachesFromTheOldParentFirst)
{
    Registry registry;
    auto oldParent = MakeEntity(registry);
    auto newParent = MakeEntity(registry);
    auto child = MakeEntity(registry);

    AttachChild(registry, oldParent, child);
    AttachChild(registry, newParent, child);

    auto const& oldParentH = registry.GetComponent<Hierarchy>(oldParent).Value().get();
    auto const& newParentH = registry.GetComponent<Hierarchy>(newParent).Value().get();
    auto const& childH = registry.GetComponent<Hierarchy>(child).Value().get();

    EXPECT_EQ(oldParentH.m_FirstChild, Entity::Null());
    EXPECT_EQ(newParentH.m_FirstChild, child);
    EXPECT_EQ(childH.m_Parent, newParent);
}

TEST(AttachChildTest, ChildWithATransform_IsMarkedDirty)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    ASSERT_TRUE(registry.AddComponent(child, Transform{}).IsOk());
    registry.GetComponent<Transform>(child).Value().get().m_Dirty = false;

    AttachChild(registry, parent, child);

    EXPECT_TRUE(registry.GetComponent<Transform>(child).Value().get().m_Dirty);
}

TEST(AttachChildTest, ChildWithNoTransform_DoesNotCrash)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);

    EXPECT_NO_THROW(AttachChild(registry, parent, child));
}

// ─── DetachChild ──────────────────────────────────────────────────────────────

TEST(DetachChildTest, OnlyChild_LeavesBothParentAndChildWithNoLinks)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, parent, child);

    DetachChild(registry, child);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& childH = registry.GetComponent<Hierarchy>(child).Value().get();
    EXPECT_EQ(parentH.m_FirstChild, Entity::Null());
    EXPECT_EQ(parentH.m_LastChild, Entity::Null());
    EXPECT_EQ(childH.m_Parent, Entity::Null());
}

TEST(DetachChildTest, FirstOfThree_ParentSFirstChildBecomesTheNextSibling)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, c);

    DetachChild(registry, a);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& bH = registry.GetComponent<Hierarchy>(b).Value().get();
    EXPECT_EQ(parentH.m_FirstChild, b);
    EXPECT_EQ(bH.m_PrevSibling, Entity::Null());
}

TEST(DetachChildTest, LastOfThree_ParentSLastChildBecomesThePreviousSibling)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, c);

    DetachChild(registry, c);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& bH = registry.GetComponent<Hierarchy>(b).Value().get();
    EXPECT_EQ(parentH.m_LastChild, b);
    EXPECT_EQ(bH.m_NextSibling, Entity::Null());
}

TEST(DetachChildTest, MiddleOfThree_SplicesItsNeighborsTogether)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, c);

    DetachChild(registry, b);

    auto const& parentH = registry.GetComponent<Hierarchy>(parent).Value().get();
    auto const& aH = registry.GetComponent<Hierarchy>(a).Value().get();
    auto const& cH = registry.GetComponent<Hierarchy>(c).Value().get();
    EXPECT_EQ(aH.m_NextSibling, c);
    EXPECT_EQ(cH.m_PrevSibling, a);
    EXPECT_EQ(parentH.m_FirstChild, a); // unaffected -- a and c are still the ends
    EXPECT_EQ(parentH.m_LastChild, c);
}

TEST(DetachChildTest, EntityWithNoHierarchyComponent_IsANoOp)
{
    Registry registry;
    auto entity = MakeEntity(registry);

    EXPECT_NO_THROW(DetachChild(registry, entity));
    EXPECT_FALSE(registry.HasComponent<Hierarchy>(entity));
}

TEST(DetachChildTest, AlreadyARoot_IsANoOp)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, parent, child);
    DetachChild(registry, child); // now a root

    EXPECT_NO_THROW(DetachChild(registry, child));
}

TEST(DetachChildTest, ChildWithATransform_IsMarkedDirty)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    ASSERT_TRUE(registry.AddComponent(child, Transform{}).IsOk());
    AttachChild(registry, parent, child);
    registry.GetComponent<Transform>(child).Value().get().m_Dirty = false;

    DetachChild(registry, child);

    EXPECT_TRUE(registry.GetComponent<Transform>(child).Value().get().m_Dirty);
}

// ─── IsAncestor ───────────────────────────────────────────────────────────────

TEST(IsAncestorTest, DirectParent_ReturnsTrue)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, parent, child);

    EXPECT_TRUE(IsAncestor(registry, parent, child));
}

TEST(IsAncestorTest, Grandparent_ReturnsTrueTransitively)
{
    Registry registry;
    auto grandparent = MakeEntity(registry);
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, grandparent, parent);
    AttachChild(registry, parent, child);

    EXPECT_TRUE(IsAncestor(registry, grandparent, child));
}

TEST(IsAncestorTest, UnrelatedEntities_ReturnsFalse)
{
    Registry registry;
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);

    EXPECT_FALSE(IsAncestor(registry, a, b));
}

TEST(IsAncestorTest, WrongDirection_DescendantIsNotItsOwnAncestorsAncestor)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, parent, child);

    EXPECT_FALSE(IsAncestor(registry, child, parent));
}

TEST(IsAncestorTest, EntityWithNoHierarchyComponent_ReturnsFalse)
{
    Registry registry;
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);

    EXPECT_FALSE(IsAncestor(registry, a, b));
}

// ─── ForEachChild ─────────────────────────────────────────────────────────────

TEST(ForEachChildTest, VisitsEveryDirectChildInSiblingOrder)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, c);

    std::vector<Entity> visited;
    ForEachChild(registry, parent, [&](Entity child) { visited.push_back(child); });

    ASSERT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], a);
    EXPECT_EQ(visited[1], b);
    EXPECT_EQ(visited[2], c);
}

TEST(ForEachChildTest, DoesNotVisitGrandchildren)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    auto grandchild = MakeEntity(registry);
    AttachChild(registry, parent, child);
    AttachChild(registry, child, grandchild);

    std::vector<Entity> visited;
    ForEachChild(registry, parent, [&](Entity c) { visited.push_back(c); });

    ASSERT_EQ(visited.size(), 1u);
    EXPECT_EQ(visited[0], child);
}

TEST(ForEachChildTest, ParentWithNoHierarchyComponent_VisitsNothing)
{
    Registry registry;
    auto parent = MakeEntity(registry);

    std::vector<Entity> visited;
    EXPECT_NO_THROW(ForEachChild(registry, parent, [&](Entity c) { visited.push_back(c); }));
    EXPECT_TRUE(visited.empty());
}

TEST(ForEachChildTest, ParentWithNoChildren_VisitsNothing)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto onlyChild = MakeEntity(registry);
    AttachChild(registry, parent, onlyChild);
    DetachChild(registry, onlyChild); // parent now has a Hierarchy but no children

    std::vector<Entity> visited;
    ForEachChild(registry, parent, [&](Entity c) { visited.push_back(c); });
    EXPECT_TRUE(visited.empty());
}

TEST(ForEachChildTest, CallableMaySafelyDetachTheChildItIsCurrentlyVisiting)
{
    // Regression guard: ForEachChild reads m_NextSibling before invoking
    // inFn, so inFn detaching (and so clearing) the current child's own
    // sibling links doesn't truncate the walk early.
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);

    std::vector<Entity> visited;
    ForEachChild(registry, parent, [&](Entity child)
    {
        visited.push_back(child);
        DetachChild(registry, child);
    });

    ASSERT_EQ(visited.size(), 2u);
    EXPECT_EQ(visited[0], a);
    EXPECT_EQ(visited[1], b);
}

// ─── DestroyEntityGraph ───────────────────────────────────────────────────────

TEST(DestroyEntityGraphTest, LeafEntity_JustDestroysItself)
{
    Registry registry;
    auto entity = MakeEntity(registry);

    DestroyEntityGraph(registry, entity);

    EXPECT_TRUE(registry.AllEntities().empty());
}

TEST(DestroyEntityGraphTest, DestroysTheWholeSubtreeIncludingGrandchildren)
{
    Registry registry;
    auto root = MakeEntity(registry);
    auto child = MakeEntity(registry);
    auto grandchild = MakeEntity(registry);
    auto unrelated = MakeEntity(registry);
    AttachChild(registry, root, child);
    AttachChild(registry, child, grandchild);

    DestroyEntityGraph(registry, root);

    auto const all = registry.AllEntities();
    EXPECT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], unrelated);
}

TEST(DestroyEntityGraphTest, DestroysEveryChildOfAMultiChildRoot)
{
    Registry registry;
    auto root = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    AttachChild(registry, root, a);
    AttachChild(registry, root, b);

    DestroyEntityGraph(registry, root);

    EXPECT_TRUE(registry.AllEntities().empty());
}

TEST(DestroyEntityGraphTest, NonRootSubtree_UnlinksItselfFromTheSurvivingParent)
{
    Registry registry;
    auto root = MakeEntity(registry);
    auto child = MakeEntity(registry);
    auto grandchild = MakeEntity(registry);
    AttachChild(registry, root, child);
    AttachChild(registry, child, grandchild);

    DestroyEntityGraph(registry, child);

    auto const& rootH = registry.GetComponent<Hierarchy>(root).Value().get();
    EXPECT_EQ(rootH.m_FirstChild, Entity::Null());
    EXPECT_EQ(rootH.m_LastChild, Entity::Null());
}

// ─── Registry::DestroyEntity and the Hierarchy ──────────────────────────────

TEST(DestroyEntityHierarchyTest, OnlyChild_ParentIsLeftWithNoChildren)
{
    // The editor's Inspector "Delete" on a child of a child crashed the
    // process: the parent kept m_FirstChild/m_LastChild pointing at the dead
    // child, and the next ForEachChild over it aborted.
    Registry registry;
    auto p1 = MakeEntity(registry);
    auto p2 = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, p1, p2);
    AttachChild(registry, p2, c);

    ASSERT_TRUE(registry.DestroyEntity(c).IsOk());

    auto const& p2H = registry.GetComponent<Hierarchy>(p2).Value().get();
    EXPECT_EQ(p2H.m_FirstChild, Entity::Null());
    EXPECT_EQ(p2H.m_LastChild, Entity::Null());

    int visited = 0;
    ForEachChild(registry, p2, [&](Entity) { ++visited; });
    EXPECT_EQ(visited, 0);
}

TEST(DestroyEntityHierarchyTest, MiddleOfThree_SiblingsAreSplicedTogether)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto c = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, c);

    ASSERT_TRUE(registry.DestroyEntity(b).IsOk());

    EXPECT_EQ(registry.GetComponent<Hierarchy>(a).Value().get().m_NextSibling, c);
    EXPECT_EQ(registry.GetComponent<Hierarchy>(c).Value().get().m_PrevSibling, a);

    std::vector<Entity> visited;
    ForEachChild(registry, parent, [&](Entity inChild) { visited.push_back(inChild); });
    EXPECT_EQ(visited, (std::vector<Entity>{ a, c }));
}

TEST(DestroyEntityHierarchyTest, ParentWithChildren_ChildrenSurviveAsRoots)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    AttachChild(registry, parent, a);
    AttachChild(registry, parent, b);

    ASSERT_TRUE(registry.DestroyEntity(parent).IsOk());

    for (auto child : { a, b })
    {
        auto const& h = registry.GetComponent<Hierarchy>(child).Value().get();
        EXPECT_EQ(h.m_Parent, Entity::Null());
        EXPECT_EQ(h.m_PrevSibling, Entity::Null());
        EXPECT_EQ(h.m_NextSibling, Entity::Null());
    }
}

TEST(DestroyEntityHierarchyTest, EntityWithNoHierarchy_StillDestroysCleanly)
{
    Registry registry;
    auto entity = MakeEntity(registry);

    EXPECT_TRUE(registry.DestroyEntity(entity).IsOk());
    EXPECT_TRUE(registry.AllEntities().empty());
}

// ─── SanitizeHierarchy ──────────────────────────────────────────────────────

Entity MakeWithHierarchy(Registry& inRegistry, Hierarchy inHierarchy = {})
{
    auto entity = MakeEntity(inRegistry);
    EXPECT_TRUE(inRegistry.AddComponent(entity, inHierarchy).IsOk());
    return entity;
}

Hierarchy& H(Registry& inRegistry, Entity inEntity)
{
    return inRegistry.GetComponent<Hierarchy>(inEntity).Value().get();
}

TEST(SanitizeHierarchyTest, SelfParent_BecomesARootAndWalkingUpTerminates)
{
    // A saved scene had an entity that was its own parent, which hung
    // Registry::IsDisabled's walk up the parent chain forever.
    Registry registry;
    auto e = MakeWithHierarchy(registry);
    H(registry, e).m_Parent = e;

    SanitizeHierarchy(registry, { e });

    EXPECT_EQ(H(registry, e).m_Parent, Entity::Null());
    EXPECT_FALSE(registry.IsDisabled(e));
}

TEST(SanitizeHierarchyTest, MutualParentCycle_IsBrokenSoExactlyOneOfThemIsARoot)
{
    Registry registry;
    auto a = MakeWithHierarchy(registry);
    auto b = MakeWithHierarchy(registry);
    H(registry, a).m_Parent = b;
    H(registry, b).m_Parent = a;

    SanitizeHierarchy(registry, { a, b });

    int roots = 0;
    for (auto e : { a, b }) if (H(registry, e).m_Parent == Entity::Null()) ++roots;
    EXPECT_EQ(roots, 1);
    EXPECT_FALSE(registry.IsDisabled(a));
    EXPECT_FALSE(registry.IsDisabled(b));
}

TEST(SanitizeHierarchyTest, ParentPointersWithNoSiblingLinks_AreRebuiltIntoAProperChain)
{
    Registry registry;
    auto parent = MakeWithHierarchy(registry);
    auto a = MakeWithHierarchy(registry, Hierarchy{ .m_Parent = parent });
    auto b = MakeWithHierarchy(registry, Hierarchy{ .m_Parent = parent });

    SanitizeHierarchy(registry, { parent, a, b });

    EXPECT_EQ(H(registry, parent).m_FirstChild, a);
    EXPECT_EQ(H(registry, parent).m_LastChild, b);
    EXPECT_EQ(H(registry, a).m_NextSibling, b);
    EXPECT_EQ(H(registry, b).m_PrevSibling, a);
    EXPECT_EQ(H(registry, a).m_PrevSibling, Entity::Null());
    EXPECT_EQ(H(registry, b).m_NextSibling, Entity::Null());
}

TEST(SanitizeHierarchyTest, ConsistentSiblingOrder_IsKeptEvenWhenItDiffersFromEntityOrder)
{
    // Sibling order decides a panel's layout slots, so it must survive.
    Registry registry;
    auto parent = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    AttachChild(registry, parent, b);
    AttachChild(registry, parent, a); // order: b, a -- opposite of creation order

    SanitizeHierarchy(registry, { parent, a, b });

    std::vector<Entity> order;
    ForEachChild(registry, parent, [&](Entity inChild) { order.push_back(inChild); });
    EXPECT_EQ(order, (std::vector<Entity>{ b, a }));
}

TEST(SanitizeHierarchyTest, ChildTheParentsChainNamesButWhoseParentIsElsewhere_IsNotClaimed)
{
    // The stale state DestroyEntity used to leave: a parent still listing
    // an entity that has since moved under another parent.
    Registry registry;
    auto p = MakeWithHierarchy(registry);
    auto q = MakeWithHierarchy(registry);
    auto x = MakeWithHierarchy(registry, Hierarchy{ .m_Parent = q });
    H(registry, p).m_FirstChild = x;
    H(registry, p).m_LastChild = x;

    SanitizeHierarchy(registry, { p, q, x });

    EXPECT_EQ(H(registry, p).m_FirstChild, Entity::Null());
    EXPECT_EQ(H(registry, q).m_FirstChild, x);
    EXPECT_EQ(H(registry, x).m_Parent, q);
}

TEST(SanitizeHierarchyTest, ParentOutsideTheGivenEntities_BecomesARoot)
{
    Registry registry;
    auto outside = MakeWithHierarchy(registry);
    auto child = MakeWithHierarchy(registry, Hierarchy{ .m_Parent = outside });

    SanitizeHierarchy(registry, { child });

    EXPECT_EQ(H(registry, child).m_Parent, Entity::Null());
}

TEST(SanitizeHierarchyTest, ParentThatIsGone_BecomesARoot)
{
    Registry registry;
    auto gone = MakeWithHierarchy(registry);
    auto child = MakeWithHierarchy(registry, Hierarchy{ .m_Parent = gone });
    ASSERT_TRUE(registry.DestroyEntity(gone).IsOk());

    SanitizeHierarchy(registry, { child });

    EXPECT_EQ(H(registry, child).m_Parent, Entity::Null());
}

TEST(SanitizeHierarchyTest, AlreadyConsistentTree_IsLeftExactlyAsItWas)
{
    Registry registry;
    auto root = MakeEntity(registry);
    auto a = MakeEntity(registry);
    auto b = MakeEntity(registry);
    auto grandchild = MakeEntity(registry);
    AttachChild(registry, root, a);
    AttachChild(registry, root, b);
    AttachChild(registry, a, grandchild);

    SanitizeHierarchy(registry, { root, a, b, grandchild });

    EXPECT_EQ(H(registry, root).m_FirstChild, a);
    EXPECT_EQ(H(registry, root).m_LastChild, b);
    EXPECT_EQ(H(registry, a).m_NextSibling, b);
    EXPECT_EQ(H(registry, a).m_FirstChild, grandchild);
    EXPECT_EQ(H(registry, grandchild).m_Parent, a);
}

TEST(SanitizeHierarchyTest, EntitiesWithNoHierarchy_AreIgnored)
{
    Registry registry;
    auto plain = MakeEntity(registry);

    SanitizeHierarchy(registry, { plain });

    EXPECT_FALSE(registry.HasComponent<Hierarchy>(plain));
}

TEST(ForEachChildTest, DanglingChildLink_StopsInsteadOfAborting)
{
    Registry registry;
    auto parent = MakeEntity(registry);
    auto child = MakeEntity(registry);
    AttachChild(registry, parent, child);

    // Force the stale state DestroyEntity used to leave behind: strip the
    // child's pools without going through the unlinking DestroyEntity.
    ASSERT_TRUE(registry.RemoveComponent<Hierarchy>(child).IsOk());

    int visited = 0;
    ForEachChild(registry, parent, [&](Entity) { ++visited; });
    EXPECT_EQ(visited, 0);
}

}
