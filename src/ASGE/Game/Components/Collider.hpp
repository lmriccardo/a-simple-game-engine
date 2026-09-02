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
 * @brief A bitmask identifying/filtering which Colliders can collide with
 *        which — see Collider::m_Layer/m_Mask and details::LayersCanCollide.
 */
using CollisionLayer = std::uint32_t;

/**
 * @brief How DetectCollisions/ResolveCollisions react to an overlap
 *        involving this Collider.
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
    ColliderShape  m_LocalBounds{};                       // shape + local offset from the owning entity's Transform
    ResolutionType m_Resolution{ ResolutionType::Solid };  // Solid (push-out) vs Trigger (sensor) vs Unknown (ignored) — see ResolutionType
    CollisionLayer m_Layer{1u};                            // which layer(s) this Collider itself occupies — a single bit by convention, but not enforced
    CollisionLayer m_Mask{ ~CollisionLayer{0} };           // which layer(s) this Collider is willing to collide with — all bits set by default, i.e. "collides with everything"
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
    return "Unknown";
}

/** @brief The read-side counterpart to ToString — any string other than "Solid"/"Trigger" maps to Unknown. */
inline ResolutionType FromString( str::StringView inSv ) noexcept
{
    if ( inSv == "Solid" ) return ResolutionType::Solid;
    if ( inSv == "Trigger" ) return ResolutionType::Trigger;
    return ResolutionType::Unknown;
}

/**
 * @brief Whether two Colliders are even willing to collide with each other,
 *        before either's actual shape/overlap is looked at — see
 *        systems::DetectCollisions, which skips a pair entirely on false.
 *
 * Mutual: inA's layer must be in inB's mask *and* inB's layer must be in
 * inA's mask, so one side excluding the other is enough to skip the pair
 * regardless of what the other side's mask says.
 */
inline bool LayersCanCollide( Collider const& inA, Collider const& inB ) noexcept
{
    return ( inA.m_Layer & inB.m_Mask ) != 0 && ( inB.m_Layer & inA.m_Mask ) != 0;
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
        table.Set<int>( "m_Layer", static_cast<int>( inCollider.m_Layer ) );
        table.Set<int>( "m_Mask",  static_cast<int>( inCollider.m_Mask ) );
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

        result.m_Resolution = details::FromString( table.Get( "m_Resolution", std::string("Solid") ) );
        result.m_Layer = static_cast<CollisionLayer>( table.Get<int>("m_Layer", 1) );
        result.m_Mask  = static_cast<CollisionLayer>( table.Get<int>("m_Mask", -1) );

        return result;
    }
};

}