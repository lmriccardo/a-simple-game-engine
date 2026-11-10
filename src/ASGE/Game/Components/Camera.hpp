#pragma once

#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Marks an entity as a camera target for systems::CameraSystem.
 *
 * Only takes effect once this entity is set as resources::ActiveCamera's
 * m_Entity — CameraSystem then reads this entity's own Transform
 * (m_WorldCoordinates) each frame as the world-space point to center the
 * view on, and this component for how to get there. Camera itself carries
 * no position of its own; the owning entity's Transform is the single
 * source of truth for it.
 */
struct Camera
{
    float m_Zoom      { 1.0f }; // uniform zoom passed straight to video::Camera::m_Zoom
    float m_Smoothing { 0.0f }; // catch-up rate toward the target position
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::Camera>
{
    static constexpr str::StringView kTableName = "Camera";

    using T = components::Camera;

    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
