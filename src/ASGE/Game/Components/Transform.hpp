#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief 2D spatial transform — position, rotation, and scale.
 *
 * m_Local* is the authored value, relative to components::Hierarchy's
 * parent (or absolute, for a root entity) — this is what round-trips
 * through Serializer<Transform> and what gameplay code should write.
 * m_World* is the flattened, absolute result systems::
 * TransformPropagationSystem computes from it, and what rendering/
 * collision should read. Setting a Local field doesn't update World by
 * itself — set m_Dirty too, so the next propagation pass picks it up.
 */
struct Transform
{
    // Serialized properties
    math::Float2 m_LocalCoordinates{0.0f, 0.0f}; // Position relative to the parent (or absolute, if a root)
    math::Float2 m_LocalScale{ 1.0f, 1.0f };     // Scale relative to the parent (or absolute, if a root)
    float m_LocalRotation{0.0f}; // radians, relative to the parent (or absolute, if a root)

    // Runtime computed coordinates
    math::Float2 m_WorldCoordinates{0.0f, 0.0f}; // Absolute position -- what rendering/collision read
    math::Float2 m_WorldScale{ 1.0f, 1.0f };     // Absolute scale -- what rendering/collision read
    float m_WorldRotation{0.0f}; // radians, absolute -- what rendering/collision read
    bool  m_Dirty{false};        // Set to request a TransformPropagationSystem recompute of World from Local
};

template<>
struct Serializer<Transform>
{
    using T = Transform;

    static constexpr str::StringView kTableName = "Transform";

    static void ToToml( Transform inTransform, asge::config::toml::TOMLTableView inTview ) noexcept;
    static T FromToml( asge::config::toml::TOMLTableView inEnttView ) noexcept;
};

}