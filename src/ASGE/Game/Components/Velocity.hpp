#pragma once

#include <ASGE/Core/Strings.hpp>
#include "Serialize.hpp"

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

template<>
struct Serializer<Velocity>
{
    using T = Velocity;

    /** @brief The subtable name ToToml/FromToml agree on — see Serializer<Transform>::kTableName. */
    static constexpr str::StringView kTableName = "Velocity";

    static void ToToml(
                            Velocity inVelocity,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    scene::SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    scene::LoadContext const& inCtx ) noexcept;
};

}
