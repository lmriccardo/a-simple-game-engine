#pragma once

#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Core/Functools.hpp>

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

/**
 * @brief Invokes inFn(child) once per direct child of inParent, in sibling
 *        order (m_FirstChild through m_LastChild). A no-op if inParent has
 *        no Hierarchy component or currently has no children. inFn is
 *        captured by the caller's own iteration, so it may safely call
 *        AttachChild/DetachChild on the entity it's currently visiting --
 *        that entity's own m_NextSibling is read before inFn runs.
 */
template<typename Callable>
requires (functools::_internal::arity_v<Callable> == 1)
    &&   (std::is_same_v<functools::_internal::arg_t<Callable, 0>, ecs::Entity>)
void ForEachChild( ecs::Registry& inReg, ecs::Entity inParent, Callable&& inFn )
{
    auto parentHResult = inReg.GetComponent<Hierarchy>(inParent);
    if ( !parentHResult ) return;
    ecs::Entity current = parentHResult.Value().get().m_FirstChild;
    while ( current != ecs::Entity::Null() )
    {
        auto currentHResult = inReg.GetComponent<Hierarchy>( current );
        ecs::Entity const next = currentHResult.Value().get().m_NextSibling;
        inFn( current );
        current = next;
    }
}

/**
 * @brief Destroys inRoot and its entire subtree (children, grandchildren,
 *        ...), depth-first via ForEachChild/recursion. Unlike a plain
 *        DestroyEntity(inRoot), this doesn't leave orphaned children behind
 *        pointing at a now-dead parent.
 */
void DestroyEntityGraph(ecs::Registry& inReg, ecs::Entity inRoot);

}

namespace asge::game::scene
{

/**
 * @brief Round-trips every entity-reference field (m_Parent/m_FirstChild/
 *        m_LastChild/m_PrevSibling/m_NextSibling) via SaveContext/
 *        LoadContext rather than the raw ecs::Entity handle.
 *
 * An Entity's index/generation isn't stable across a save/load round trip
 * -- Load() creates fresh entities in whatever order it encounters
 * `[[entity]]` blocks, so a handle saved from the old registry would
 * silently alias onto the wrong entity (or none at all) in the new one.
 * ToToml writes each field as SaveContext::Resolve's stable per-entity id
 * (-1 for Entity::Null()); FromToml reads that id back through
 * LoadContext::Resolve, which only works correctly once every referenced
 * entity has already been created — see SceneSerializer::Load, which
 * creates every entity up front in one pass before populating any
 * component in a second pass, specifically so a Hierarchy field can point
 * to a sibling/parent entity regardless of which is loaded first.
 */
template<>
struct Serializer<components::Hierarchy>
{
    static constexpr str::StringView kTableName = "Hierarchy";

    using T = components::Hierarchy;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}