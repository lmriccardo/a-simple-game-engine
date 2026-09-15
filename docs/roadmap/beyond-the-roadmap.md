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

## 7. ECS view/system scoping — excluding entities from a shared Registry

[03-ECS Phase 1](phase-1/03.1-entity-component-system.md) and its
[Phase 2](phase-2/03.2-entity-component-system.md) follow-on were both
checked: Phase 1 shipped "basic query/iteration (view entities that have a
given set of components)" and nothing more; Phase 2 is entirely about
storage layout (archetypes) and parallel scheduling, not query expressivity.
`ecs::View<Ts...>` only supports "has all of `Ts...`" — there's no way to
also say "and does *not* have this marker component," so a system like
`systems::MovementSystem` run against a shared `Registry` moves *every*
matching entity, with no way to carve out one that needs different
(e.g. collision-aware) handling. The only workaround today is giving
different kinds of entities their own private `Registry` instances, which
stops being viable once a game wants everything in one shared world.
Reported as [#36](https://github.com/lmriccardo/a-simple-game-engine/issues/36).

## 8. Cross-Registry render-order interleaving

[10-Rendering System Phase 1](phase-1/10.1-rendering-system.md) is explicit
that `Sprite::m_Layer`/`m_YSort` sort one `DrawItem` batch — built from one
`RenderSystem(registry, renderer)` call's own `Registry` — and nothing else;
there's no task anywhere about ordering draw calls *across* two separate
`RenderSystem` calls each over its own `Registry`. Whichever call happens to
run second in a frame always draws entirely on top of the first, regardless
of any per-entity layer or Y value on either side, so two registries whose
entities need to visually interleave (e.g. a car in one `Registry` passing
behind a tree in another) currently need a hand-fixed, scene-specific draw
order. Related to #7 above but distinct — that's filtering *within* one
`Registry`, this is ordering *across* several. Reported as
[#65](https://github.com/lmriccardo/a-simple-game-engine/issues/65).

## 9. Broad-phase collision detection

[07-Physics Phase 1](phase-1/07.1-physics-system.md) is the one item here
that *is* named in a roadmap doc, but only to explicitly punt on it:
"broadphase beyond a naive all-pairs check (revisit only if
[13-Optimization](phase-1/13.1-optimization.md) profiling shows it's
needed)." Checking 13-Optimization across all three of its phase docs,
though, turns up nothing — it's CPU-profiling infrastructure
(`TimingProfiler`/`ScopedTimer`) and instrumentation habits, never a task
to actually build a broad-phase step. So the punt has no real owner:
`systems::DetectCollisions` tests every `Collider`-bearing entity pair
exactly once via a plain nested loop, `CollisionLayer` masks filter pairs
only *after* they're formed, and there's no spatial grid/quadtree/
sweep-and-prune anywhere in the engine. Fine for the handful of colliders
any current example uses; stops scaling once a game wants dozens-to-hundreds
colliding at once. Reported as
[#66](https://github.com/lmriccardo/a-simple-game-engine/issues/66).

## 10. Curve/spline math primitive

`asge::math`'s entire geometry surface — checked, along with every phase
file for "curve"/"spline"/"bezier" — is `Vec2`/`Rect`/`Circle` plus pairwise
`PenetrationVector` ([Core/Math/Geometry/Collision](../../src/ASGE/Core/Math/Geometry/Collision.hpp)).
Nothing represents an authored path (a curving street, a patrol route, a
cutscene camera move) as data, and there's no way to sample a position or
direction along one. This is a genuine math-module gap, not a physics or
rendering one — it's needed for path-*following* (a fixed, known route),
distinct from graph-search pathfinding, which no phase file covers either
but which also has no reported need yet. Reported as
[#67](https://github.com/lmriccardo/a-simple-game-engine/issues/67), with a
proposed `Spline` shape (Catmull-Rom through authored waypoints,
`PointAt`/`TangentAt`, arc-length parameterization).

## 11. `Transform::m_Rotation` is never applied by rendering

`Transform::m_Rotation` exists, defaults to 0, and round-trips through
`Serializer<Transform>` — but nothing reads it back out.
`Sprite::SpriteGetDstRect` builds its destination rect from
`m_X`/`m_Y`/`m_ScaleX`/`m_ScaleY` only, and `RenderSystem` always calls
`IRenderer`'s axis-aligned `DrawTexture` overload, never the
`DrawTextureAffine` one already capable of expressing rotation. The only
"rotation" tracked anywhere in the roadmap is
[10-Rendering System Phase 1](phase-1/10.1-rendering-system.md)'s open 2D
camera task ("position, zoom, rotation applied to `DrawX` calls") — a
*global* camera rotation, not routing an individual entity's own
`Transform::m_Rotation` through to its drawn `Sprite`. Set it today and a
sprite silently keeps drawing upright. Reported as
[#68](https://github.com/lmriccardo/a-simple-game-engine/issues/68).

## Priority, if picking one

Items #1 and #3 above have since shipped. Of what's left, **#9 (broad-phase
collision)** is the one most likely to bite silently — nothing here fails
loudly, a scene just gets slower as colliding entities grow, with no
profiling task anywhere flagged to catch it. **#7 (view/system scoping)**
is the one most likely to be felt next by anyone building a real game on a
single shared `Registry`, per the issue that reported it.
