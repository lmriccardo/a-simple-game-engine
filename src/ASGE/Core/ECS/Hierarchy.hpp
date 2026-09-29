#pragma once

#include <concepts>
#include "Registry.hpp"

namespace asge::ecs::components
{

/**
 * @brief An entity's place in the scene tree — one parent link plus a
 *        doubly-linked sibling list among that parent's children.
 *
 * Built and maintained exclusively through AttachChild()/DetachChild();
 * an entity with no Hierarchy component, or one whose m_Parent is
 * Entity::Null(), is a root as far as TransformPropagationSystem is
 * concerned.
 */
struct Hierarchy
{
    asge::ecs::Entity m_Parent      = asge::ecs::Entity::Null(); // This entity's parent, or Null() if it's a root
    asge::ecs::Entity m_FirstChild  = asge::ecs::Entity::Null(); // Head of this entity's own children list
    asge::ecs::Entity m_LastChild   = asge::ecs::Entity::Null(); // Tail of this entity's own children list
    asge::ecs::Entity m_NextSibling = asge::ecs::Entity::Null(); // Next child under the same parent
    asge::ecs::Entity m_PrevSibling = asge::ecs::Entity::Null(); // Previous child under the same parent
};

/**
 * @brief Makes inChild a child of inParent, re-parenting it (detaching from
 *        any previous parent first) if it already had one. A no-op if
 *        inParent equals inChild, or if inChild is already an ancestor of
 *        inParent — either would introduce a cycle. Lazily adds a Hierarchy
 *        to either entity that doesn't have one yet, and marks inChild's
 *        Transform (if any) dirty so the next TransformPropagationSystem
 *        pass recomposes its world transform under the new parent.
 */
void AttachChild( ecs::Registry& inReg, ecs::Entity inParent, ecs::Entity inChild );

/**
 * @brief Unlinks inChild from its parent and siblings, leaving it a root.
 *        A no-op if inChild has no Hierarchy component, or has one but is
 *        already a root. Marks inChild's Transform (if any) dirty, same as
 *        AttachChild(), since its world transform is no longer composed
 *        through the old parent.
 */
void DetachChild( ecs::Registry& inReg, ecs::Entity inChild );

/**
 * @brief True if inPotentialAncestor appears somewhere in inEntity's chain
 *        of parents (its parent, its parent's parent, ...). False if
 *        inEntity has no Hierarchy component, or is a root.
 */
bool IsAncestor( ecs::Registry& inReg, ecs::Entity inPotentialAncestor, ecs::Entity inEntity );

/**
 * @brief Invokes inFn(child) once per direct child of inParent, in sibling
 *        order (m_FirstChild through m_LastChild). A no-op if inParent has
 *        no Hierarchy component or currently has no children. inFn is
 *        captured by the caller's own iteration, so it may safely call
 *        AttachChild/DetachChild on the entity it's currently visiting --
 *        that entity's own m_NextSibling is read before inFn runs.
 *
 * Constrained via std::invocable rather than functools' function_trait,
 * since inFn is allowed to be a generic lambda (e.g. `[](auto child){...}`)
 * -- a generic call operator has no fixed argument type to extract until
 * it's actually invoked, so invocability with ecs::Entity is the only
 * thing that can honestly be checked here.
 */
template<typename Callable>
requires std::invocable<Callable, ecs::Entity>
void ForEachChild( ecs::Registry& inReg, ecs::Entity inParent, Callable&& inFn )
{
    auto parentHResult = inReg.GetComponent<Hierarchy>(inParent);
    if ( !parentHResult ) return;
    ecs::Entity current = parentHResult.Value().get().m_FirstChild;
    while ( current != ecs::Entity::Null() )
    {
        auto currentHResult = inReg.GetComponent<Hierarchy>( current );
        if ( !currentHResult ) break; // a dead entity left linked in: stop rather than abort
        ecs::Entity const next = currentHResult.Value().get().m_NextSibling;
        inFn( current );
        current = next;
    }
}

/**
 * @brief Rebuilds every Hierarchy link among inEntities from their m_Parent
 *        pointers, so data that was never validated (a loaded scene file)
 *        can't leave the tree inconsistent or cyclic.
 *
 * A parent that is the entity itself, isn't in inEntities, or has no
 * Hierarchy is dropped (the entity becomes a root), and a parent cycle is
 * broken at the point it would close. Sibling order is kept from each
 * parent's stored first-child chain wherever that chain is consistent;
 * children it doesn't reach follow in inEntities order.
 */
void SanitizeHierarchy( ecs::Registry& inReg, std::vector<ecs::Entity> const& inEntities );

/**
 * @brief Destroys inRoot and its entire subtree (children, grandchildren,
 *        ...), depth-first via ForEachChild/recursion. Unlike a plain
 *        DestroyEntity(inRoot), which detaches its children and leaves them
 *        alive as roots, this destroys them too.
 */
void DestroyEntityGraph(ecs::Registry& inReg, ecs::Entity inRoot);

}