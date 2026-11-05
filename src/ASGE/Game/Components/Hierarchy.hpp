#pragma once

#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Core/Functools.hpp>

namespace asge::game::components
{

/**
 * @brief Makes inChild a child of inParent.
 *
 * Calls the Core ECS AttachChild (see Core/ECS/Hierarchy.hpp), which
 * re-parents the child if it already has a parent and ignores a request that
 * would create a cycle, then marks the child's Transform (if any) dirty so
 * its world transform is recomposed under the new parent.
 */
void AttachChild( ecs::Registry& inReg, ecs::Entity inParent, ecs::Entity inChild );

/**
 * @brief Detaches inChild from its parent, leaving it a root.
 *
 * Calls the Core ECS DetachChild, then marks the child's Transform (if any)
 * dirty since its world transform is no longer composed through the old
 * parent.
 */
void DetachChild(ecs::Registry &inReg, ecs::Entity inChild);

}

namespace asge::game::scene
{

/**
 * @brief Round-trips every entity-reference field via SaveContext/LoadContext.
 *
 * The fields are m_Parent/m_FirstChild/m_LastChild/m_PrevSibling/m_NextSibling,
 * and they are saved as stable ids rather than the raw ecs::Entity handle.
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
struct Serializer<ecs::components::Hierarchy>
{
    static constexpr str::StringView kTableName = "Hierarchy"; // Name of the TOML subtable (de)serialization uses

    using T = ecs::components::Hierarchy;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}