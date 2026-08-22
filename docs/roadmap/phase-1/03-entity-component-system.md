# ECS (Entity Component System) — Phase 1

*Priority: Tier 1 — next up. See [README](../README.md). Other phases:
[Phase 2](../phase-2/03-entity-component-system.md) (archetypes, parallel systems).*

## Goals
Give gameplay code a way to compose behavior out of small, reusable data components
instead of one bespoke `IGame` subclass per demo. Scoped for a 2D engine: this only
needs enough plumbing to attach a `Transform` + `Sprite` + something-that-reacts-to-input
to an id and iterate over the result each frame — not a general-purpose engine-agnostic
ECS library.

## Design notes
- **Entity**: an opaque id — index + generation counter — not a pointer or object, so a
  stale handle (entity destroyed, slot reused) can be detected instead of silently
  aliasing a different entity.
- **Component storage**: a simple per-type sparse-set / dense-vector store
  (`std::vector<T>` + an index lookup keyed by `EntityId`), not a full archetype/chunked
  system. Archetype storage is a performance optimization; nothing in this engine has
  enough entities yet to justify it — see [Phase 2](../phase-2/03-entity-component-system.md).
- **Systems**: plain functions operating on a view/query result, not a special `System`
  base class or scheduler. Keep it simple until something concrete demands more
  structure — parallel execution is Phase 2, not Phase 1.
- **No serialization or reflection layer yet.** [06-Scene Management](06-scene-management.md)
  will define what actually needs to round-trip (which components, in what format) when
  it gets there; building a generic reflection system now would be guessing.

## Tasks
- [x] Entity manager (create/destroy, generation-counter ids, alive checks)
- [x] Component storage (attach/detach/get/has, per-type)
- [x] Basic query/iteration (view entities that have a given set of components)
- [x] Systems as plain functions taking a view — no base class, no scheduler

## Components (2D-scoped)
- `Transform` — position (`math::Float2`), rotation, scale
- `Sprite` — an `ITexture` handle + optional source `math::Rect`, drawn via the
  existing `DrawTexture` family (see [02-Rendering Primitives](../02-rendering-primitives.md))
- `Collider` — shape + bounds; feeds [07-Physics](07-physics-system.md) once that starts
- `Rigidbody` — velocity/mass; also feeds 07-Physics
- `AudioSource` — feeds [08-Audio](08-audio-system.md)
- `Camera` — feeds [10-Rendering System](../10-rendering-system.md) Step 1
- `Script`/behavior hook — deferred; doesn't mean much beyond a raw function pointer
  until [11-Scripting & Events](11-scripting-and-events.md)'s event dispatcher exists

Dropped `MeshRenderer` and `Camera` for 3D-style rendering from the original template
list — this engine's equivalent of "renderable" is `Sprite`, drawn through the
`ITexture`/`DrawTexture` primitives that already exist.

## Milestones

### Step 1 — Entity lifecycle

*Status:* ✅ **Done**

- `EntityId`: index + generation counter
- Create/destroy, `IsAlive(id)`
- Unit tests: create/destroy round-trip, stale-handle detection after an index is
  destroyed and reused

### Step 2 — Component storage & queries

*Status:* ✅ **Done**

- Attach/detach/get/has, at minimum for `Transform` and `Sprite`
- A minimal view/query API (entities that have a given component set) — needs to be
  correct and usable first, not zero-overhead
- Unit tests: attach/detach round-trip, a view only yields entities that actually have
  every requested component

### Step 3 — Integration demo

*Status:* ✅ **Done**

- New `examples/ecs_demo`: a real `IGame` driving a `Registry` of seven entities
  (`Transform` + `Velocity` + `Sprite`), moved each frame by `MovementSystem` and
  drawn by iterating the view and calling `DrawTexture` (`RenderSystem`) — one
  entity is WASD-controlled, the rest drift and wrap around the screen
- Proves the API works end-to-end wired into a real `IGame`, not just in isolation
  under a unit test

## Deliverables
- `EntityId` type + create/destroy API
- Component attach/detach/get/has + a basic view/query API
- `Transform` and `Sprite` wired into at least one real example
- Unit tests for entity lifecycle and component storage, held to the same
  real-behavior-not-a-fake standard as the rendering test suite

## Explicitly out of scope for Phase 1
Archetypes, parallel systems, and serialization — see
[Phase 2](../phase-2/03-entity-component-system.md). Serialization specifically is
blocked on [06-Scene Management](06-scene-management.md), which doesn't exist yet and
would only get guessed at prematurely. A general `Script`/behavior-component runtime is
blocked on [11-Scripting & Events](11-scripting-and-events.md).
