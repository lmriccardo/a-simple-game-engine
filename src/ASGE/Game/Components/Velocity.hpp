#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Linear velocity, in units per second.
 *
 * The rate of change of position; MovementSystem applies it to the Transform.
 */
struct Velocity
{
    float m_DX{0.0f}; // change in X per second
    float m_DY{0.0f}; // change in Y per second
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips both fields (m_DX, m_DY).
 *
 * Velocity carries no runtime-only state beyond them.
 */
template<>
struct Serializer<components::Velocity>
{
    using T = components::Velocity;

    static constexpr str::StringView kTableName = "Velocity"; // Name of the TOML subtable (de)serialization uses

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            components::Velocity inVelocity,
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
