#pragma once

#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::components
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

}