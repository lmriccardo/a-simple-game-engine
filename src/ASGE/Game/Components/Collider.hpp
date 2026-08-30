#pragma once

#include <variant>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Strings.hpp>
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
using ColliderShape = std::variant<math::Rect, math::Circle>;

struct Collider
{
    ColliderShape m_LocalBounds{};
};

/**
 * @brief Serializer for math::Rect shapes
 */
template<>
struct Serializer<math::Rect>
{
    static constexpr str::StringView kShapeName = "Rect";

    static void ToToml( 
        math::Rect inShape, asge::config::TOMLTableView inTview 
    ) noexcept  {
        inTview.Set("m_Width",   inShape.w)
               .Set("m_Height",  inShape.h)
               .Set("m_OffsetX", inShape.x)
               .Set("m_OffsetY", inShape.y);
    }

    static math::Rect FromToml( asge::config::TOMLTableView inTview ) noexcept
    {
        return math::Rect{
            inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f),
            inTview.Get("m_Width", 0.0f), inTview.Get("m_Height", 0.0f)
        };
    }
};

/**
 * @brief Serializer for math::Circle shapes
 */
template<>
struct Serializer<math::Circle>
{
    static constexpr str::StringView kShapeName = "Circle";

    static void ToToml( 
        math::Circle inShape, asge::config::TOMLTableView inTview 
    ) noexcept  {
        inTview.Set("m_OffsetX", inShape.m_Center.x())
               .Set("m_OffsetY", inShape.m_Center.y())
               .Set("m_Radius",  inShape.m_Radius);
    }

    static math::Circle FromToml( asge::config::TOMLTableView inTview ) noexcept
    {
        return math::Circle{
            math::Float2{ inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f) },
            inTview.Get("m_Radius",  0.0f)
        };
    }
};

template<>
struct Serializer<Collider>
{
    using T = Collider;
    static constexpr str::StringView kTableName = "Collider";

    static void ToToml( 
        Collider inCollider, asge::config::TOMLTableView inTview 
    ) noexcept {
        auto table = inTview.Table(std::string(kTableName));
        std::visit( [&table]( auto const& inShape )
        {
            using ShapeT = std::decay_t<decltype( inShape )>;
            table.Set<str::String>( "m_Shape", str::String(Serializer<ShapeT>::kShapeName) );
            Serializer<ShapeT>::ToToml( inShape, table );
        }, inCollider.m_LocalBounds);
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto table = inEnttView.Table(std::string(kTableName));
        auto shapeKind = table.Get("m_Shape", std::string("Rect"));
        
        Collider result{};

        if ( shapeKind == Serializer<math::Rect>::kShapeName )
        {
            result.m_LocalBounds = Serializer<math::Rect>::FromToml( table );
        }
        else
        {
            result.m_LocalBounds = Serializer<math::Circle>::FromToml( table );
        }

        return result;
    }
};

}