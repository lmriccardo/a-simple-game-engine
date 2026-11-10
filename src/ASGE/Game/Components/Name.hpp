#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief A human-readable label for an entity.
 *
 * Purely for debugging/editor display, never read by any gameplay system.
 */
struct Name
{
    str::String m_Name{}; // Freeform display label; empty means "unnamed"
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Name only.
 *
 * See Serializer<components::Transform>::kTableName for the subtable-naming
 * contract.
 */
template<>
struct Serializer<components::Name>
{
    static constexpr str::StringView kTableName = "Name"; // Name of the TOML subtable (de)serialization uses

    using T = components::Name;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}