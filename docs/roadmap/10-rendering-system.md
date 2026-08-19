# Rendering System (2D)

## Goals
Extend the 2D primitives from [02](02-rendering-primitives.md) with camera control,
compositing effects, and — if a real feature ever needs it — a programmable-shader path.
Not a 3D pipeline: no mesh renderer, no PBR, no ray tracing. `SDLRenderer` stays the
primary backend throughout; nothing here replaces it unless Step 3 explicitly decides to.

## Tasks
- [ ] 2D camera (position, zoom, rotation applied to `DrawX` calls)
- [ ] Render-target compositing helpers (tint, additive glow, fade) on the existing
      `SDL_Renderer` backend — no shader work required
- [ ] Decision gate: is there an actual feature that needs custom shading?
- [ ] `SDL_GPU` backend spike (new `SDLGPURenderer`, coexisting with `SDLRenderer`)
- [ ] Shader system (blocked on the spike above)
- [ ] Material system — shader + params + texture bindings (blocked on shader system)

## Milestones

### Step 1 — Camera & fixed-function effects

*Status:* Not started. No blockers, but deprioritized behind
[03-ECS](03-entity-component-system.md) and [04-Input](04-input-system.md) — an MVP
needs entities and input before it needs camera zoom or a glow effect.

- 2D camera: position/zoom/rotation, applied when issuing draw calls
- Compositing helpers built on `SDL_Renderer`'s existing render-target + color-mod +
  blend-mode support (tint, additive blend, fade) — proves out several "shader-ish"
  effects with zero backend risk

### Step 2 — Decision gate

Not a build milestone — a checkpoint. Revisit `SDL_GPU` only once a concrete feature
(lighting, particles, a specific visual effect) actually needs custom shading logic
that Step 1's fixed-function tricks can't cover. If nothing ever needs it, Steps 3-5
stay skipped indefinitely; that's a valid outcome, not a stalled roadmap item.

### Step 3 — `SDL_GPU` backend spike

- Stand up a second `IRenderer`-conformant backend (`SDLGPURenderer`) against SDL3's
  `SDL_GPU` API, alongside `SDLRenderer`, not replacing it
- Minimum bar: clear + one textured quad + one custom shader effect, ideally
  headless-testable the same way `SDLRenderer` is
- Shaders precompiled per-backend (SPIR-V/DXIL/MSL) via `SDL_shadercross`
- Exit decision: dual-backend permanently (SDL_Renderer for everyday drawing,
  SDL_GPU only for the effect that triggered this), or full swap, or the spike
  reveals it isn't worth it and Step 2's gate stays closed

### Step 4 — Shader system

Only after Step 3 lands. A minimal way to attach a compiled shader + uniforms to a
draw call through `IRenderer`, scoped to whatever triggered Step 2 — not a general
shader authoring pipeline.

### Step 5 — Material system

Shader + parameters + texture bindings as a reusable object, wired into
`DrawTexture`/`DrawString`-equivalent calls. Only worth doing once Step 4 has more
than one consumer.

## Deliverables
- `IRenderer` extended with camera-aware draw calls
- Compositing helper API for tint/glow/fade built on the current backend
- A written decision (in this doc) on whether/when `SDL_GPU` was adopted, and why

## Explicitly out of scope
Mesh renderer, material/lighting systems scoped for 3D, PBR, GPU instancing of
meshes, deferred rendering/shadows/HDR, ray tracing, global illumination. These are
3D-pipeline concerns carried over from an earlier generic template and don't apply
to a 2D SDL engine; revisit only if the engine's scope changes.
