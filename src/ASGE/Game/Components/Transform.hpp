#pragma once

#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief 2D spatial transform — position, rotation, and scale.
 */
struct Transform
{
    float m_X{0.0f};
    float m_Y{0.0f};
    float m_Rotation{0.0f}; // radians
    float m_ScaleX{1.0f};
    float m_ScaleY{1.0f};
};

template<>
struct Serializer<Transform>
{
    using T = Transform;

    static void ToToml(
        Transform inTransform, asge::config::TOMLTableView inTview
    ) noexcept {
        inTview.Table("Transform")
               .Set("m_X", inTransform.m_X)
               .Set("m_Y", inTransform.m_Y)
               .Set("m_Rotation", inTransform.m_Rotation)
               .Set("m_ScaleX", inTransform.m_ScaleX)
               .Set("m_ScaleY", inTransform.m_ScaleY);
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto table = inEnttView.Table("Transform");

        Transform result{};
        result.m_X        = table.Get("m_X", result.m_X);
        result.m_Y        = table.Get("m_Y", result.m_Y);
        result.m_Rotation = table.Get("m_Rotation", result.m_Rotation);
        result.m_ScaleX   = table.Get("m_ScaleX", result.m_ScaleX);
        result.m_ScaleY   = table.Get("m_ScaleY", result.m_ScaleY);
        return result;
    }
};

}