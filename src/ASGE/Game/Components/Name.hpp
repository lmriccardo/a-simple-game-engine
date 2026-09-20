#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief A human-readable label for an entity — purely for debugging/editor
 *        display, never read by any gameplay system.
 */
struct Name
{
    str::String m_Name{}; // Freeform display label; empty means "unnamed"
};

}

namespace asge::game::scene
{

/** @brief Round-trips m_Name only — see Serializer<components::Transform>::kTableName for the subtable-naming contract. */
template<>
struct Serializer<components::Name>
{
    static constexpr str::StringView kTableName = "Name";

    using T = components::Name;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}