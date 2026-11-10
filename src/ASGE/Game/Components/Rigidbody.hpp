#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Marks an entity as physics-driven.
 *
 * Subject to gravity and collision-resolution push-out, as opposed to
 * entities that merely have a Velocity (e.g. scripted movement) without being
 * a physics body.
 */
struct Rigidbody
{
    float m_Mass{1.0f};            // Relative mass; the heavier body yields less in a push-out
    bool  m_AffectedByGravity{true}; // Whether GravitySystem accelerates this entity downwards
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips both fields (m_Mass, m_AffectedByGravity).
 *
 * Rigidbody carries no runtime-only state beyond them.
 */
template<>
struct Serializer<components::Rigidbody>
{
    using T = components::Rigidbody;
    static constexpr str::StringView kTableName = "Rigidbody"; // Name of the TOML subtable (de)serialization uses

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            components::Rigidbody inRigidbody,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}