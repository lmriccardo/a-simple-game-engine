#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

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

}

namespace asge::game::scene
{

/** @brief Round-trips both fields (m_Mass, m_AffectedByGravity) -- Rigidbody carries no runtime-only state beyond them. */
template<>
struct Serializer<components::Rigidbody>
{
    using T = components::Rigidbody;
    static constexpr str::StringView kTableName = "Rigidbody";

    static void ToToml(
                            components::Rigidbody inRigidbody,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}