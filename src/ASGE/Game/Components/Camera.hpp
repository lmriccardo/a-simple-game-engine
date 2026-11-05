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
    float m_Smoothing { 0.0f }; // catch-up rate toward the target position; 0 snaps there immediately, higher values ease in faster (see CameraSystem)
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Zoom and m_Smoothing.
 *
 * See AudioSource's Serializer doc comment for why a scene file only ever
 * describes this much and not any runtime-only state (Camera has none of its
 * own).
 */
template<>
struct Serializer<components::Camera>
{
    static constexpr str::StringView kTableName = "Camera"; // Name of the TOML subtable (de)serialization uses

    using T = components::Camera;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
