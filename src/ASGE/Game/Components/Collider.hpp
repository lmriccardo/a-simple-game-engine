#pragma once

#include <variant>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Strings.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/** @brief The set of shapes a Collider can be — see AabbOverlap/PenetrationVector (Collision.hpp). */
using ColliderShape = std::variant<math::Rect, math::Circle>;

/**
 * @brief How DetectCollisions/ResolveCollisions react to an overlap
 *        involving this Collider.
 *
 * See systems::DetectCollisions/ResolveCollisions/DispatchTriggerEvents
 * (PhysicsSystem.hpp) for exactly how each value changes an overlapping
 * pair's handling.
 */
enum class ResolutionType
{
    Unknown, // Unrecognized/not-yet-configured — DetectCollisions ignores the pair entirely (no push-out, no trigger event)
    Trigger, // Non-blocking sensor — never pushed out or pushes anything out; overlap fires events::OnCollisionTriggerEnter/Stay/Exit instead
    Solid    // The default — participates in normal push-out collision response
};

/**
 * @brief A hitbox used for collision detection — a Rect or a Circle.
 *
 * Independent of Sprite — an entity's hitbox doesn't have to match its
 * drawn size. Its world position is Transform.m_X/m_Y (top-left, same
 * convention as RenderSystem) offset by the shape's own local origin
 * (Rect's x/y, or Circle's m_Center).
 */
struct Collider
{
    ColliderShape  m_LocalBounds{};                    // shape + local offset from the owning entity's Transform
    ResolutionType m_Resolution{ ResolutionType::Solid }; // Solid (push-out) vs Trigger (sensor) vs Unknown (ignored) — see ResolutionType
};

namespace details
{

/** @brief ResolutionType -> its TOML representation — see Serializer<Collider>::ToToml. */
inline str::String ToString( ResolutionType inResolveT ) noexcept
{
    switch ( inResolveT )
    {
    case ResolutionType::Solid   : return "Solid";
    case ResolutionType::Trigger : return "Trigger";
    case ResolutionType::Unknown : return "Unknown";
    }
    return "Unknown"; // Unreachable via a value actually named above; keeps this well-defined
                       // if ResolutionType's underlying value ever comes from somewhere else.
}

/** @brief The read-side counterpart to ToString — any string other than "Solid"/"Trigger" maps to Unknown. */
inline ResolutionType FromString( str::StringView inSv ) noexcept
{
    if ( inSv == "Solid" ) return ResolutionType::Solid;
    if ( inSv == "Trigger" ) return ResolutionType::Trigger;
    return ResolutionType::Unknown;
}

}

/**
 * @brief Serializer for a Rect-shaped Collider.
 *
 * kShapeName is what Serializer<Collider> writes/reads as the "m_Shape"
 * discriminator, so FromToml knows which of Rect/Circle to parse the rest
 * of the table as.
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

/** @brief Serializer for a Circle-shaped Collider — see Serializer<math::Rect>::kShapeName. */
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

        table.Set<str::String>( "m_Resolution", details::ToString( inCollider.m_Resolution ) );
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto table = inEnttView.Table(std::string(kTableName));
        // Defaults to "Rect" so a scene file saved before Circle existed --
        // no "m_Shape" key at all -- still parses as a Rect, unchanged.
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

        // Defaults to "Solid" so a scene file saved before m_Resolution
        // existed -- no "m_Resolution" key at all -- still parses as Solid
        // (Collider's own in-code default), not Unknown; an Unknown
        // Collider is silently excluded from all collision handling (see
        // DetectCollisions), so a missing key must not resolve to it.
        result.m_Resolution = details::FromString( table.Get( "m_Resolution", std::string("Solid") ) );

        return result;
    }
};

}