#pragma once 

#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::systems
{

/**
 * @brief Integrates every entity's Velocity into its Transform's position.
 *
 * Plain Euler integration: position += velocity * inDeltaTime, for every
 * entity with both a Transform and a Velocity. No collision awareness —
 * see CollisionResolution for what runs after this each frame.
 */
void MovementSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Separates every overlapping Solid pair of Colliders; fires a
 *        signal instead for a pair involving a Trigger.
 *
 * Naive all-pairs check over every Transform+Collider entity, dispatching
 * on each pair's actual shapes (Rect and/or Circle — see
 * math::PenetrationVector) via std::visit. What happens once a pair
 * overlaps depends on each side's Collider::m_Resolution:
 *   - Either side `Unknown`: ignored entirely — no push-out, no event.
 *   - Either side `Trigger`: no push-out at all (even if the other side is
 *     `Solid` and movable) — instead emits events::OnTriggerOverlap(),
 *     trigger entity first when only one side is one. See that signal's
 *     own doc comment for a callback's constraints (it fires synchronously,
 *     mid-iteration over this same Registry).
 *   - Both sides `Solid` (the default): pushed apart as before. An entity
 *     is only pushed if it has both a Velocity and a Rigidbody; anything
 *     missing either (e.g. static level geometry) is treated as immovable.
 *     When both overlapping entities are movable, the correction is split
 *     by mass ratio — the heavier body yields less — rather than evenly.
 *     Zeroes velocity on whichever axis was corrected, so a resolved
 *     entity doesn't immediately re-penetrate next frame.
 */
void CollisionResolution( ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Accelerates every Rigidbody+Velocity entity downward by gravity.
 *
 * Adds kGravity * inDeltaTime to m_DY for every entity with a Rigidbody
 * whose m_AffectedByGravity is true; entities without a Rigidbody, or with
 * gravity disabled on it, are left untouched.
 */
void GravitySystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

}