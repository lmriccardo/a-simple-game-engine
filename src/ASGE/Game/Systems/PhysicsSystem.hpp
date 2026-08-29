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
 * @brief Separates every overlapping pair of Colliders by pushing them apart.
 *
 * Naive all-pairs AABB check over every Transform+Collider entity (see
 * math::PenetrationVector). An entity without a Velocity is treated as
 * immovable (e.g. static level geometry) and never gets pushed; when both
 * overlapping entities have one, the correction is split evenly between
 * them. Zeroes velocity on whichever axis was corrected, so a resolved
 * entity doesn't immediately re-penetrate next frame.
 */
void CollisionResolution( ecs::Registry& inRegistry ) noexcept;

}