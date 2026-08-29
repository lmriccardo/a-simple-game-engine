#pragma once

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief Axis-aligned bounding box used for collision detection.
 *
 * Independent of Sprite — an entity's hitbox doesn't have to match its
 * drawn size. The box's world position is Transform.m_X/m_Y (top-left,
 * same convention as RenderSystem) offset by m_OffsetX/m_OffsetY.
 */
struct Collider
{
    math::Rect m_LocalBounds{};
};

template<>
struct Serializer<Collider>
{
    using T = Collider;
    static constexpr std::string_view kTableName = "Collider";

    static void ToToml( 
        Collider inCollider, asge::config::TOMLTableView inTview 
    ) noexcept  {
        inTview.Table(std::string(kTableName))
               .Set("m_Width",   inCollider.m_LocalBounds.w)
               .Set("m_Height",  inCollider.m_LocalBounds.h)
               .Set("m_OffsetX", inCollider.m_LocalBounds.x)
               .Set("m_OffsetY", inCollider.m_LocalBounds.y);
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto table = inEnttView.Table(std::string(kTableName));
        Collider result{};
        result.m_LocalBounds = math::Rect{
            table.Get("m_OffsetX", 0.0f),
            table.Get("m_OffsetY", 0.0f),
            table.Get("m_Width", 0.0f),
            table.Get("m_Height", 0.0f)
        };

        return result;
    }
};

}