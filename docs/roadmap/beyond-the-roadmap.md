# Beyond the Roadmap

*Meta doc, no phase — like [00-overview](00-overview.md) and this folder's
[README](README.md). Written 2026-08-30, prompted by physics work landing on
`mils/0.7.0-physics-system` and the question "to create a game, what do I
still need that isn't even in the roadmap?"*

The systems tracked in `phase-N/` are real and each individually well-scoped,
but none of them — including their Phase 2/3 follow-ons — cover the pieces
below. These aren't speculative feature requests; each one was checked
against every phase file that could plausibly own it before being listed
here, so this doc only holds things that are genuinely untracked, not things
that are merely unbuilt.

## 1. Game-state / screen flow - ✅ complete

[`Application`](../../src/ASGE/Application/Application.hpp) holds exactly one
`game::IGame&` for its entire run — there's no way to swap it or stack it.
[Scene Management](phase-1/06.1-scene-management.md) (`SceneManager`) only
manages *which TOML file's entities are loaded into the Registry* — one
active scene at a time, entity-data only. Neither gives a way to express
"title screen → gameplay → pause menu layered on top of gameplay (without
unloading it) → game over screen." Today that would have to be one `IGame`
subclass with a hand-rolled `enum` switch inside `Update`/`Render` — every
example avoids this because none of them have more than one screen yet.
Scene Management's own Phase 2/3 scope (prefabs, additive loading, world
streaming) doesn't cover it either — those are still about scene *data*, not
which screen/state is currently driving the game loop.

## 2. Sprite/frame animation - ✅ complete

Every phase file was checked for "animation": the only hits were UI widget
property tweening ([09.2-ui-framework](phase-2/09.2-ui-framework.md),
position/opacity interpolation for retained-mode widgets) and a passing
mention of "physics + AI + animation" as a hypothetical justification for ECS
archetypes ([03.2-entity-component-system](phase-2/03.2-entity-component-system.md)).
Nothing covered playing back a spritesheet — a walk cycle, an idle/attack
state switch — until `components::Animation` + `systems::AnimationSystem`
landed; see [10.1-rendering-system](phase-1/10.1-rendering-system.md), which
now tracks it alongside layer-based draw ordering.

## 3. Trigger/sensor colliders and collision layers - ✅ complete

Checked all three [Physics](phase-1/07.1-physics-system.md) phase docs — CCD,
raycasts, a character controller
([Phase 2](phase-2/07.2-physics-system.md)), soft body and vehicle physics
([Phase 3](phase-3/07.3-physics-system.md)) are all named explicitly, but
nothing covers a non-blocking "sensor" `Collider` (pickups, damage zones,
level-exit triggers) or layer/mask-based filtering (e.g. "player bullets
ignore the player"). `Collider`+`Rigidbody` exist now; a sensor/trigger
concept and layer filtering are usually the very next thing reached for once
solid-body collision works, and neither is on any list yet.

## 4. A visual editor

[00-overview](00-overview.md) states "Editor integration" as a **Main
Priority** — and no phase file, anywhere, ever picks it up. The only
"editor" mentioned in the whole roadmap is a
[Phase 3 UI-styling editor](phase-3/09.3-ui-framework.md) for retained-mode
widgets, not a level/scene editor for placing entities and tuning
`Transform`/`Collider`/`Sprite` visually. Right now the only way to build a
scene is hand-writing TOML — fine while content is small, but it's the
overview doc's own stated priority with nothing tracking it.

## 5. Gameplay timers/coroutines/tweening

`TimingProfiler`/`ScopedTimer`
([01-Core Engine](phase-1/01.1-core-engine.md)) measure performance, not
gameplay time. [Scripting & Events](phase-2/11.2-scripting-and-events.md)
gives a `Script` component and an event bus — good for reacting to things,
not for "wait 1.5s, then do X" or "move from A to B over 2s" sequencing, which
most cutscene/juice/spawn-timer gameplay code relies on.

## 6. Save-game / player progress persistence

Zero mentions anywhere. `SceneSerializer`/`SceneManager`
([Scene Management](phase-1/06.1-scene-management.md)) round-trip *level*
content — entities and their components — which is a different concern from
"which levels are unlocked," "high score," or "settings the player changed":
data that outlives any one scene and isn't itself an ECS `Registry`.

## Priority, if picking one

**#1 (game-state flow)** is the one likely to be felt first — even a minimal
physics playground benefits from "menu → play → reset without restarting the
process." **#3 (triggers/layers)** is the one that follows most directly from
the physics work in progress right now.
