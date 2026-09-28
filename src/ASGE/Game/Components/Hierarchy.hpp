#pragma once

#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Core/Functools.hpp>

namespace asge::game::components
{

void AttachChild( ecs::Registry& inReg, ecs::Entity inParent, ecs::Entity inChild );
void DetachChild(ecs::Registry &inReg, ecs::Entity inChild);

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
struct Serializer<ecs::components::Hierarchy>
{
    static constexpr str::StringView kTableName = "Hierarchy";

    using T = ecs::components::Hierarchy;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}