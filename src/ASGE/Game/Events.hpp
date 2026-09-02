#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::events
{

/**
 * @brief Fires the first frame a Trigger Collider pair starts overlapping.
 *
 * A process-wide signal (not per-Registry) fired synchronously from
 * systems::DispatchTriggerEvents — see its doc comment for exactly when
 * this fires vs. OnCollisionTriggerStay/Exit. If exactly one side of the
 * pair is a Trigger, it's reported first (see CollisionContact).
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerEnter() noexcept;

/**
 * @brief Fires the frame a previously-overlapping Trigger pair stops
 *        overlapping — including when that's because one side was
 *        destroyed. See OnCollisionTriggerEnter for the other half.
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerExit() noexcept;

/**
 * @brief Fires every frame after the first that a Trigger pair keeps
 *        overlapping. See OnCollisionTriggerEnter for the other half.
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerStay() noexcept;

}