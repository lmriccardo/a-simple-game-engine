# Simple Game Engine Roadmap

This roadmap organizes the game engine into modular systems. Each system's doc is
split by **phase** — `phase-1/` is that system's MVP-appropriate scope, `phase-2/`
and `phase-3/` are progressively more advanced/speculative follow-ons, each in its
own file so a system's later phases don't dilute what's actually needed now. A
system whose entire scope is a single phase (done, or gate/step-based rather than
maturity-tiered) stays a flat file at the root instead of a single-entry folder.
Every phase file contains:
- Goals (scoped to that phase)
- Current state (grounded in actual code where it exists)
- Design notes
- Tasks
- Deliverables
- Explicitly out of scope (pointing at whichever phase picks it up)

## Recommended implementation order

Reordered 2026-08-19, restructured into phase folders 2026-08-22. The original list
was a straight 1-13 sequence inherited from a generic engine template; several
entries (asset pipeline, UI) sat later than what they actually unblock, and 02.2 was
scoped as a full 3D pipeline nothing in this engine needs. This version is tiered
around one goal: reach a genuinely playable 2D MVP — entities, input, a scene, and
something on screen that responds to a player — before spending time on systems that
only pay off once that MVP exists. **Tier** (below) is priority/order — when to work
on a system; **Phase** (within each system's docs) is scope/maturity — how much of
that system to build. The two are independent: a Tier-4/deferred system still has
its own "what would Phase 1 look like" doc, it's just not scheduled.

### Tier 0 — Foundation (done)
1. **[Core Engine](01-core-engine.md)** — ✅ complete
2. **[Rendering Primitives](02-rendering-primitives.md)** — ✅ complete (shapes, textures, text)

### Tier 1 — MVP-critical (do next, in this order)
3. **[ECS](phase-1/03-entity-component-system.md)** (create/destroy entities, attach
   components) — everything below needs somewhere to hang a `Transform`, `Sprite`,
   `Collider`, etc. This was the biggest unblock in the whole roadmap. - ✅ complete
4. **[Input](phase-1/04-input-system.md)** — keyboard, mouse, and a queryable
   polling API, all wired end-to-end with a runtime-verified example
   (`examples/input_demo`). - ✅ complete
5. **[Asset Pipeline](phase-1/05-asset-pipeline.md)** (file importing, asset
   references) — moved ahead of Scene Management. `Image::Load`/`Font::Load` are
   already two independent ad-hoc loaders; formalize a common asset-handle concept
   before a third (audio) shows up and triplicates the pattern, and before
   Scene/ECS need a way to reference assets by handle rather than a hardcoded path
   per demo.
6. **[Scene Management](phase-1/06-scene-management.md)** (load/save scenes) — now
   meaningfully assembles ECS entities + asset references into something bigger than
   one hand-written `IGame` per example.

### Tier 2 — Makes it feel like a game
7. **[Physics](phase-1/07-physics-system.md)** (AABB collision, gravity, collision
   response) — first system that makes gameplay feel like gameplay; depends on ECS's
   `Transform`/`Rigidbody`/`Collider` components.
8. **[Audio](phase-1/08-audio-system.md)** (play sounds, volume) — low coupling to
   everything else, but wants the Asset Pipeline's loading pattern for WAV/OGG first.
9. **[UI Framework](phase-1/09-ui-framework.md)** (immediate-mode UI, debug
   overlays) — moved up from its original position. This is nearly free right now:
   immediate-mode UI is just `DrawRect` + `DrawString`, both of which already exist.
   A debug overlay (FPS, entity inspector) also pays for itself immediately as a dev
   tool, not just an MVP feature.

### Tier 3 — Revisit once Tier 1-2 exist and something demands it
10. **[Rendering System (2D)](10-rendering-system.md)** — Step 1 (camera,
    fixed-function compositing) once there's a scene to point a camera at. Steps 2-5
    (`SDL_GPU`, shaders, materials) stay behind their own decision gate and may never
    trigger; see that file. Kept as a single Step/gate-based doc rather than phase
    folders — its steps are sequential and conditionally gated, not independent
    maturity levels.
11. **[Scripting & Events](phase-1/11-scripting-and-events.md)** (event dispatcher)
    — worth doing once enough ECS systems exist that they need decoupling from each
    other. The dispatch primitive (`asge::signals::Signal`) already exists; this
    phase is building the game-event bus on top of it. Managed scripting
    (Lua/C#/Python) is well past MVP.

### Tier 4 — Explicitly deferred
12. **[Networking](phase-1/12-networking.md)** — the MVP is single-player. Revisit
    only if multiplayer becomes an actual goal, not speculatively.
13. **[Optimization](phase-1/13-optimization.md)** — not a terminal step to save for
    last and then do once. Basic CPU profiling is already done as part of Core
    Engine (`TimingProfiler`/`ScopedTimer`); a dedicated deep pass (job system, GPU
    profiling, cache optimization) only makes sense once there's enough real content
    to profile, i.e. post-MVP.
14. **[Documentation](phase-1/14-documentation.md)** — same shape as Optimization:
    light and continuous (keep doc comments and this roadmap current as you go)
    rather than a single pass bolted on at the very end.

## Folder layout

```
docs/roadmap/
  00-overview.md, README.md          - meta docs, no phase
  01-core-engine.md                  - done, single phase
  02-rendering-primitives.md         - done, single phase
  10-rendering-system.md             - step/gate-based, not phase-tiered
  phase-1/0N-system.md               - every other system's MVP-appropriate scope
  phase-2/0N-system.md               - next increment beyond MVP (where one exists)
  phase-3/0N-system.md               - further/speculative (where one exists)
```
