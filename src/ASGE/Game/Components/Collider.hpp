#pragma once

#include <variant>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

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
str::String ToString( ResolutionType inResolveT ) noexcept;

/** @brief The read-side counterpart to ToString — any string other than "Solid"/"Trigger" maps to Unknown. */
ResolutionType FromString( str::StringView inSv ) noexcept;

/**
 * @brief Whether two Colliders are even willing to collide with each other,
 *        before either's actual shape/overlap is looked at — see
 *        systems::DetectCollisions, which skips a pair entirely on false.
 *
 * Mutual: inA's layer must be in inB's mask *and* inB's layer must be in
 * inA's mask, so one side excluding the other is enough to skip the pair
 * regardless of what the other side's mask says.
 */
bool LayersCanCollide( Collider const& inA, Collider const& inB ) noexcept;

}

}

namespace asge::game::scene
{

template<>
struct Serializer<components::Collider>
{
    using T = components::Collider;
    static constexpr str::StringView kTableName = "Collider";

    static void ToToml(
                            components::Collider inCollider,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}