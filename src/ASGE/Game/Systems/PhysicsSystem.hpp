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
 * @brief Returns inCollider's local shape moved into world space.
 *
 * The shape is offset by inTransform's world position — same shape kind, new
 * coordinates.
 */
components::ColliderShape WorldBounds(
    components::Transform const& inTransform, components::Collider const& inCollider ) noexcept;

/**
 * @brief Which entities of a canonical EntityPair were Triggers when the overlap was recorded.
 *
 * Bit 0 is m_First and bit 1 is m_Second; see kFirstIsTrigger and
 * kSecondIsTrigger.
 */
using TriggerSides = std::uint8_t;
inline constexpr TriggerSides kFirstIsTrigger  = 1u << 0; // m_First of the pair was a Trigger
inline constexpr TriggerSides kSecondIsTrigger = 1u << 1; // m_Second of the pair was a Trigger

/**
 * @brief Cross-frame bookkeeping PhysicsUpdate needs.
 *
 * It remembers which Trigger pairs were overlapping last frame (so
 * CollisionSystem can tell Enter from Stay) and which side of each was the
 * Trigger (so a per-entity Exit still reaches it if its resolution changed
 * mid-overlap).
 */
struct PhysicsState
{
    std::map<ecs::EntityPair, TriggerSides> m_PreviousTriggerPairs; // Trigger pairs overlapping last frame
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
 * @brief Runs a full physics frame.
 *
 * Gravity, movement, path following, then collision detection, response and
 * trigger events, in that order. inState persists across calls (one per
 * Registry) so CollisionSystem can tell Enter from Stay from Exit.
 */
void PhysicsUpdate( ecs::Registry& inRegistry, PhysicsState& inState, float inDeltaTime ) noexcept;

/**
 * @brief Moves each PathFollow entity along its parent's components::Path.
 *
 * Applies the follower's offsets and optional tangent alignment, and always
 * writes the tangent into PathFollow::m_Heading. Emits events::OnPathFinished
 * at the end of a non-looping Path, warns once per follower whose parent has
 * no Path, and skips followers paused with components::PathFollowPause.
 */
void PathFollowingSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

}