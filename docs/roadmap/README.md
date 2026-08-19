# Simple Game Engine Roadmap

This roadmap organizes the game engine into modular systems.
Each markdown file contains:
- Goals
- Architecture
- Milestones
- Deliverables
- Stretch goals

## Recommended implementation order

Reordered 2026-08-19. The original list was a straight 1-13 sequence inherited from a
generic engine template; several entries (asset pipeline, UI) sat later than what they
actually unblock, and 02.2 was scoped as a full 3D pipeline nothing in this engine needs.
This version is tiered around one goal: reach a genuinely playable 2D MVP — entities,
input, a scene, and something on screen that responds to a player — before spending time
on systems that only pay off once that MVP exists.

### Tier 0 — Foundation (done)
1. **[Core Engine](01-core-engine.md)** — ✅ complete
2. **[Rendering Primitives](02-rendering-primitives.md)** — ✅ complete (shapes, textures, text)

### Tier 1 — MVP-critical (do next, in this order)
3. **[ECS](03-entity-component-system.md)**, Phase 1 only (create/destroy entities, attach
   components) — everything below needs somewhere to hang a `Transform`, `Sprite`,
   `Collider`, etc. This is the biggest unblock in the whole roadmap right now.
4. **[Input](04-input-system.md)**, finish Phase 1 — keyboard is already wired through
   `SystemEvent`; `MouseEvent` is declared in the enum but missing from the event variant.
   Small, and blocks anything interactive.
5. **[Asset Pipeline](05-asset-pipeline.md)**, Basic tier only (file importing, asset
   references) — moved ahead of Scene Management. `Image::Load`/`Font::Load` are already
   two independent ad-hoc loaders; formalize a common asset-handle concept before a third
   (audio) shows up and triplicates the pattern, and before Scene/ECS need a way to
   reference assets by handle rather than a hardcoded path per demo.
6. **[Scene Management](06-scene-management.md)**, Phase 1 only (load/save scenes) — now
   meaningfully assembles ECS entities + asset references into something bigger than one
   hand-written `IGame` per example.

### Tier 2 — Makes it feel like a game
7. **[Physics](07-physics-system.md)**, Basic tier (AABB collision, gravity, collision
   response) — first system that makes gameplay feel like gameplay; depends on ECS's
   `Transform`/`Rigidbody`/`Collider` components.
8. **[Audio](08-audio-system.md)**, Phase 1 (play sounds, volume) — low coupling to
   everything else, but wants the Asset Pipeline's loading pattern for WAV/OGG first.
9. **[UI Framework](09-ui-framework.md)**, Basic tier (immediate-mode UI, debug overlays)
   — moved up from its original position. This is nearly free right now: immediate-mode
   UI is just `DrawRect` + `DrawString`, both of which already exist. A debug overlay
   (FPS, entity inspector) also pays for itself immediately as a dev tool, not just an
   MVP feature.

### Tier 3 — Revisit once Tier 1-2 exist and something demands it
10. **[Rendering System (2D)](10-rendering-system.md)** — Step 1 (camera,
    fixed-function compositing) once there's a scene to point a camera at. Steps 2-5
    (`SDL_GPU`, shaders, materials) stay behind their own decision gate and may never
    trigger; see that file.
11. **[Scripting & Events](11-scripting-and-events.md)**, Phase 1 (event dispatcher)
    — worth doing once enough ECS systems exist that they need decoupling from each
    other. Managed scripting (Lua/C#/Python) is well past MVP.

### Tier 4 — Explicitly deferred
12. **[Networking](12-networking.md)** — the MVP is single-player. Revisit only if
    multiplayer becomes an actual goal, not speculatively.
13. **[Optimization](13-optimization.md)** — not a terminal step to save for last and
    then do once. Keep it continuous and lightweight (basic profiling as soon as it's
    cheap to add) throughout every tier above; a dedicated deep pass (job system, GPU
    profiling, cache optimization) only makes sense once there's enough real content to
    profile, i.e. post-MVP.
14. **[Documentation](14-documentation.md)** — same shape as Optimization: light and
    continuous (keep doc comments and this roadmap current as you go) rather than a
    single pass bolted on at the very end.
