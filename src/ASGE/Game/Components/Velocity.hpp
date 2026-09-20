#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Linear velocity — rate of change of position, in units per second.
 */
struct Velocity
{
    float m_DX{0.0f}; // change in X per second
    float m_DY{0.0f}; // change in Y per second
};

}

namespace asge::game::scene
{

/** @brief The subtable name ToToml/FromToml agree on — see Serializer<components::Transform>::kTableName. */
template<>
struct Serializer<components::Velocity>
{
    using T = components::Velocity;

    static constexpr str::StringView kTableName = "Velocity";

    static void ToToml(
                            components::Velocity inVelocity,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
