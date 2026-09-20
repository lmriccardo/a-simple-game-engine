#pragma once

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief 2D spatial transform — position, rotation, and scale.
 */
struct Transform
{
    // Serialized properties
    math::Float2 m_LocalCoordinates{0.0f, 0.0f};
    math::Float2 m_LocalScale{ 1.0f, 1.0f };
    float m_LocalRotation{0.0f}; // radians

    // Runtime computed coordinates
    math::Float2 m_WorldCoordinates{0.0f, 0.0f};
    math::Float2 m_WorldScale{ 1.0f, 1.0f };
    float m_WorldRotation{0.0f}; // radians
    bool  m_Dirty{false};
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