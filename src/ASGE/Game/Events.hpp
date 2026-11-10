#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::events
{

/**
 * @brief Fires the first frame a Trigger Collider pair starts overlapping.
 *
 * A process-wide signal (not per-Registry) fired synchronously from
 * systems::CollisionSystem — see its doc comment for exactly when
 * this fires vs. OnCollisionTriggerStay/Exit. If exactly one side of the
 * pair is a Trigger, it's reported first.
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerEnter() noexcept;

/**
 * @brief Fires the frame a previously-overlapping Trigger pair stops overlapping.
 *
 * Also fires when that's because one side was destroyed. See
 * OnCollisionTriggerEnter for the other half.
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerExit() noexcept;

/**
 * @brief Fires every frame after the first that a Trigger pair keeps overlapping.
 *
 * See OnCollisionTriggerEnter for the other half.
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnCollisionTriggerStay() noexcept;

/**
 * @brief Fires when a non-looping Animator state reaches its last frame.
 *
 * The owning entity is passed along. A process-wide signal fired
 * synchronously from systems::AnimatorSystem, once per playback of a state;
 * looping states never emit it.
 */
signals::Signal<ecs::Entity>& OnAnimationFinished() noexcept;

/**
 * @brief Fires when a non-looping PathFollow reaches the end of its Path.
 *
 * A process-wide signal, fired synchronously by systems::PathFollowingSystem
 * with the follower entity when PathFollow::m_Finished becomes true. It fires
 * again only if m_Finished is reset and the end is reached again; followers on
 * a looping Path never emit it.
 */
signals::Signal<ecs::Entity>& OnPathFinished() noexcept;

}
