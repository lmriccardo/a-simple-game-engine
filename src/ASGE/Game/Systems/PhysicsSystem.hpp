#pragma once 

#include <set>
#include <vector>
#include <span>
#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Math/Math.hpp>

namespace asge::game::systems
{

/**
 * @brief Cross-frame bookkeeping PhysicsUpdate needs — currently just which
 *        Trigger pairs were overlapping last frame, so DispatchTriggerEvents
 *        can tell a new overlap (Enter) from a continuing one (Stay).
 */
struct PhysicsState
{
    std::set<ecs::EntityPair> m_PreviousTriggerPairs;
};

/**
 * @brief One overlapping Collider pair found by DetectCollisions.
 *
 * m_Penetration is signed to move m_Entity1 away from m_Entity2, matching
 * math::PenetrationVector's convention (Collision.hpp) — negate it to move
 * m_Entity2 instead. If exactly one side is a Trigger, it's always
 * m_Entity1, regardless of which one DetectCollisions actually found first.
 */
struct CollisionContact
{
    ecs::Entity  m_Entity1, m_Entity2;
    math::Float2 m_Penetration;
    bool         m_IsTrigger;
};

/**
 * @brief Integrates every entity's Velocity into its Transform's position.
 *
 * Plain Euler integration: position += velocity * inDeltaTime, for every
 * entity with both a Transform and a Velocity. No collision awareness —
 * see DetectCollisions/ResolveCollisions for what runs after this each frame.
 */
void MovementSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Finds every overlapping Transform+Collider pair this frame.
 *
 * Naive all-pairs check, dispatching on each pair's actual shapes (Rect
 * and/or Circle — see math::PenetrationVector) via std::visit. A pair with
 * an Unknown ResolutionType on either side is skipped entirely — not
 * reported as a contact at all, Solid or Trigger.
 */
std::vector<CollisionContact> DetectCollisions( ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Pushes apart every non-Trigger contact from inContacts.
 *
 * An entity only counts as movable if it has both a Velocity and a
 * Rigidbody; anything missing either (e.g. static level geometry) is
 * treated as immovable, and a pair where neither side is movable is
 * skipped. When both are movable, the correction is split by mass ratio —
 * the heavier body yields less — rather than evenly. Zeroes velocity on
 * whichever axis was corrected, so a resolved entity doesn't immediately
 * re-penetrate next frame.
 */
void ResolveCollisions( ecs::Registry& inRegistry, std::span<CollisionContact const> inContacts ) noexcept;

/**
 * @brief Fires events::OnCollisionTrigger{Enter,Stay,Exit} for every Trigger contact.
 *
 * A pair overlapping this frame that wasn't in inState's previous-frame set
 * fires Enter; one that was fires Stay. A pair in the previous-frame set but
 * absent from inContacts fires Exit — including when that's because one
 * side was destroyed since last frame, not just because it moved away.
 * Updates inState's previous-frame set to this frame's before returning.
 */
void DispatchTriggerEvents( PhysicsState& inState, std::span<CollisionContact const> inContacts ) noexcept;

/**
 * @brief Accelerates every Rigidbody+Velocity entity downward by gravity.
 *
 * Adds kGravity * inDeltaTime to m_DY for every entity with a Rigidbody
 * whose m_AffectedByGravity is true; entities without a Rigidbody, or with
 * gravity disabled on it, are left untouched.
 */
void GravitySystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Runs a full physics frame: gravity, movement, then collision
 *        detection/response/trigger-events, in that order.
 *
 * The single entry point a game loop actually needs — see GravitySystem/
 * MovementSystem/DetectCollisions/ResolveCollisions/DispatchTriggerEvents
 * for what each step does on its own. inState persists across calls (one
 * per Registry, not shared) so DispatchTriggerEvents can tell Enter from
 * Stay from Exit.
 */
void PhysicsUpdate( ecs::Registry& inRegistry, PhysicsState& inState, float inDeltaTime ) noexcept;

}