# UI Framework — Phase 1

*Priority: Tier 2 — nearly free right now. See [README](../README.md).
Other phases: [Phase 2](../phase-2/09-ui-framework.md) (retained-mode UI,
animation), [Phase 3](../phase-3/09-ui-framework.md) (styling, UI editor).*

## Goals
Immediate-mode UI and debug overlays — a dev tool that pays for itself immediately,
not just an MVP feature.

## Current state
No UI framework code exists yet, but its only two real dependencies already do:
`IRenderer::DrawRect` and text drawing via `Font`/`ITexture`
([02-Rendering Primitives](../02-rendering-primitives.md)), plus
[04-Input Phase 1](../phase-1/04-input-system.md)'s `InputState` for mouse
hit-testing. Immediate-mode UI is fundamentally `DrawRect` + `DrawString` + a mouse
position/click check every frame — all three pieces already exist.

## Design notes
- **Immediate-mode, not retained-mode.** No widget tree, no persistent UI objects —
  a button is a function called once per frame that draws a rect + label and
  returns whether it was clicked this frame (via
  [InputState](../phase-1/04-input-system.md)'s `IsMouseButtonPressed` +
  `GetMousePosition` hit-test), the classic immediate-mode pattern. Matches this
  phase's "nearly free" framing — no new persistent state to manage.
- **Debug overlay (FPS, entity inspector) as the first real consumer** — it's a dev
  tool, not gameplay UI, so it's low-risk to build first and immediately useful
  (FPS from [01-Core Engine](../01-core-engine.md)'s `time::DeltaTime()`, entity
  count/inspector from [03-ECS](03-entity-component-system.md)'s `Registry`).

## Tasks
- [ ] Immediate-mode primitives: label, button, checkbox/toggle — each a plain
      function taking `IRenderer&` + `InputState const&`, drawing itself and
      returning its current interaction state
- [ ] Debug overlay: FPS counter, entity count (from
      [03-ECS](../phase-1/03-entity-component-system.md)'s `Registry`)
- [ ] A minimal layout helper (stack widgets vertically/horizontally) — just enough
      to avoid hand-computing every rect's position

## Deliverables
- At least one example showing an immediate-mode debug overlay (FPS + entity count)
- A button/toggle usable from example code without hand-rolling hit-testing

## Explicitly out of scope for Phase 1
Retained-mode UI, animation support ([Phase 2](../phase-2/09-ui-framework.md));
CSS-like styling, a UI editor ([Phase 3](../phase-3/09-ui-framework.md)).
