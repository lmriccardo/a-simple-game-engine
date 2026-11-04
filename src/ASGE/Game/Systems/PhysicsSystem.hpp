#pragma once

#include <map>
#include <set>
#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Collider.hpp>

namespace asge::game::systems
{

/**
 * @brief inCollider's local shape offset by inTransform's position, in
 *        world space — same shape kind, new coordinates.
 */
components::ColliderShape WorldBounds(
    components::Transform const& inTransform, components::Collider const& inCollider ) noexcept;

/** @brief Which entities of a canonical EntityPair were Triggers when the overlap was recorded (bit 0: m_First, bit 1: m_Second). */
using TriggerSides = std::uint8_t;
inline constexpr TriggerSides kFirstIsTrigger  = 1u << 0;
inline constexpr TriggerSides kSecondIsTrigger = 1u << 1;

/**
 * @brief Cross-frame bookkeeping PhysicsUpdate needs — which Trigger pairs
 *        were overlapping last frame (so CollisionSystem can tell Enter from
 *        Stay) and which side of each was the Trigger (so a per-entity Exit
 *        still reaches it if its resolution changed mid-overlap).
 */
struct PhysicsState
{
    std::map<ecs::EntityPair, TriggerSides> m_PreviousTriggerPairs;
};

/**
 * @brief Integrates every entity's Velocity into its Transform's position.
 *
 * Plain Euler integration: position += velocity * inDeltaTime, for every
 * entity with both a Transform and a Velocity. No collision awareness —
 * see CollisionSystem for what runs after this each frame.
 */
void MovementSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Detects, resolves and reports every Collider overlap this frame.
 *
 * A QuadTree broad phase feeds a shape-accurate narrow phase. Pairs whose layer/mask
 * can't collide, with an Unknown resolution, or on the same body are skipped. Solid
 * pairs are pushed apart (mass-weighted, only bodies with Velocity+Rigidbody move);
 * Trigger pairs fire Enter/Stay/Exit through ioState once the traversal is over.
 */
void CollisionSystem( ecs::Registry& inRegistry, PhysicsState& ioState ) noexcept;

/**
 * @brief Accelerates every Rigidbody+Velocity entity downward by gravity.
 *
 * Adds kGravity * inDeltaTime to m_DY for every entity with a Rigidbody
 * whose m_AffectedByGravity is true; entities without a Rigidbody, or with
 * gravity disabled on it, are left untouched.
 */
void GravitySystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Runs a full physics frame: gravity, movement, path following, then
 *        collision detection/response/trigger-events, in that order.
 *
 * The single entry point a game loop actually needs — see GravitySystem/
 * MovementSystem/PathFollowingSystem/CollisionSystem for what each step does
 * on its own. inState persists across calls (one per Registry, not shared) so
 * CollisionSystem can tell Enter from Stay from Exit.
 */
void PhysicsUpdate( ecs::Registry& inRegistry, PhysicsState& inState, float inDeltaTime ) noexcept;

/**
 * @brief Moves every PathFollow entity along the components::Path of its
 *        parent (via Hierarchy), at m_Speed units/second.
 *
 * Skips followers with no parent, a parent without a Path, a Path not yet
 * resolved into segments, or already m_Finished. The sampled point plus
 * m_OffsetH/m_OffsetV becomes the follower's local position (relative to
 * the route entity), and, when m_AlignRotation is set, its local rotation faces the
 * path tangent unless the segment is degenerate. At the end a looping Path wraps; otherwise
 * m_Traveled clamps and m_Finished is set. Marks the Transform m_Dirty.
 */
void PathFollowingSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

}