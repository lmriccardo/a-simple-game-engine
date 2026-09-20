#pragma once

#include <ASGE/Core/Strings.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief Marks an entity as physics-driven: subject to gravity and
 * collision-resolution push-out, as opposed to entities that merely have
 * a Velocity (e.g. scripted movement) without being a physics body.
 */
struct Rigidbody
{
    float m_Mass{1.0f};
    bool  m_AffectedByGravity{true};
};

template<>
struct Serializer<Rigidbody>
{
    using T = Rigidbody;
    static constexpr str::StringView kTableName = "Rigidbody";

    static void ToToml(
                            Rigidbody inRigidbody,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    scene::SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    scene::LoadContext const& inCtx ) noexcept;
};

}