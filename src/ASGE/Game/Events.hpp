#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::events
{

/**
 * @brief Fires when two Colliders overlap and at least one is a Trigger —
 *        see systems::CollisionResolution (Game/Systems/PhysicsSystem.hpp).
 *
 * A process-wide signal (one instance for the whole program, like
 * FileWatcher's own signals), not a per-Registry one — connect once (e.g.
 * in a game's constructor, keeping the returned signals::Connection alive
 * for as long as the callback should stay registered) rather than
 * reconnecting every frame. Slot signature is `(ecs::Entity inA,
 * ecs::Entity inB)`: when only one side of the pair is a Trigger, inA is
 * always that Trigger and inB the other entity; when both sides are
 * Triggers the order is unspecified.
 *
 * @warning Fires every frame the pair is still overlapping — a "stay"
 *          event, not "enter" once — so a listener that only cares about
 *          the first frame of overlap needs its own has-fired tracking.
 * @warning Emitted synchronously from inside CollisionResolution's own
 *          all-pairs iteration over the Registry's Transform+Collider view.
 *          A listener must not mutate the Registry from this callback
 *          (destroying an entity, adding/removing a component) — that
 *          iteration is still in progress and DestroyEntity's swap-and-pop
 *          removal (Registry.cpp) can invalidate it mid-loop. Queue such
 *          changes and apply them after CollisionResolution returns
 *          instead (see examples/physics_demo's despawn-zone handling).
 */
signals::Signal<ecs::Entity, ecs::Entity>& OnTriggerOverlap() noexcept;

}