# Scene Management — Phase 1

*Priority: Tier 1 — after Asset Pipeline. See [README](../README.md). Other phases:
[Phase 2](../phase-2/06-scene-management.md) (prefabs, additive loading),
[Phase 3](../phase-3/06-scene-management.md) (world streaming).*

## Goals
Assemble [03-ECS](03-entity-component-system.md) entities and
[05-Asset Pipeline](05-asset-pipeline.md) asset references into something bigger
than one hand-written `IGame` per example — a scene that can be loaded and saved.

## Current state
Nothing exists yet. Every example today builds its entities/components directly in
`IGame`'s constructor or first `Update`/`Render` call (see `ecs_demo::SpawnEntities`)
— there's no concept of a scene as data, only as code.

## Design notes
- **This phase is what actually defines ECS serialization**, not the other way
  around. [03-ECS Phase 1](03-entity-component-system.md) deliberately built no
  serialization/reflection layer, noting scene management would drive the real
  requirements (which components, in what format) once it exists — that's this doc.
  Guessing at a generic reflection system before this phase would have been
  premature; this phase is where that guess gets replaced with an actual answer.
- **Depends on [05-Asset Pipeline Phase 1](../phase-1/05-asset-pipeline.md)'s
  asset-handle concept** — a scene needs to reference assets (which texture a
  `Sprite` uses) by something more durable than a hardcoded path per demo.
- **Load/save, not a generic serialization framework.** Scoped to "can a scene's
  entities+components round-trip to a file and back", not a reflection system that
  serializes arbitrary C++ types.

## Tasks
- [ ] Define what a "scene" is: a set of entities + their components + asset
      references, in some concrete file format (TOML is already parsed by
      [01-Core Engine](../01-core-engine.md)'s config system — reusing that parser
      over inventing a new format is the obvious first thing to try)
- [ ] Scene load: file → populated `Registry` ([03-ECS](03-entity-component-system.md))
- [ ] Scene save: populated `Registry` → file
- [ ] Feed the concrete serialization requirements this uncovers back into
      [03-ECS Phase 2](../phase-2/03-entity-component-system.md)

## Deliverables
- A scene file format (even a minimal one) that at least one example's entity set
  can round-trip through
- Load/save API taking/producing a `Registry`

## Explicitly out of scope for Phase 1
Prefabs and additive loading ([Phase 2](../phase-2/06-scene-management.md)), world
streaming ([Phase 3](../phase-3/06-scene-management.md)).
