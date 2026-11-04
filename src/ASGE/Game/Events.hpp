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

/**
 * @brief Fires when a non-looping Animation reaches its last frame and stops
 *        (Animation::m_Playing cleared), with the owning entity.
 *
 * A process-wide signal fired synchronously from systems::AnimationSystem,
 * once per playback; looping animations never emit it.
 */
signals::Signal<ecs::Entity>& OnAnimationFinished() noexcept;

/**
 * @brief Fires when a non-looping PathFollow reaches the end of its Path
 *        (PathFollow::m_Finished flips to true), with the follower entity.
 *
 * A process-wide signal fired synchronously from systems::PathFollowingSystem,
 * exactly once per run: it fires on the frame m_Finished becomes true, and
 * again only if m_Finished is later reset and the end is reached again.
 * Followers on a looping Path never finish, so they never emit it.
 */
signals::Signal<ecs::Entity>& OnPathFinished() noexcept;

}
