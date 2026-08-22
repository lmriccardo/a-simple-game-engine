# UI Framework — Phase 2

*Priority: Tier 2 follow-on — worth it once real gameplay UI (menus, HUDs) is
needed. See [README](../README.md). Other phases:
[Phase 1](../phase-1/09-ui-framework.md) (immediate-mode, debug overlays),
[Phase 3](../phase-3/09-ui-framework.md) (styling, UI editor).*

## Goals
Persistent UI objects (menus, HUDs) that don't need to be redeclared every frame,
and animated transitions between UI states.

## Design notes
- **Retained-mode is a different structure, not an extension of Phase 1's
  functions** — persistent widget objects with their own state, likely composed of
  ECS entities+components ([03-ECS](03-entity-component-system.md)) rather than a
  bespoke widget hierarchy, matching how everything else in this engine composes.
- Only worth building once gameplay actually needs UI that outlives a single frame's
  call (a pause menu, an inventory) — [Phase 1](../phase-1/09-ui-framework.md)'s
  immediate-mode functions cover debug tooling fine on their own.

## Tasks
- [ ] Retained-mode widget representation (entities + UI-specific components, or an
      equivalent persistent structure)
- [ ] Basic animation: interpolate a widget property (position, opacity) over time

## Deliverables
- At least one example with a persistent UI element (e.g. a menu) built in
  retained mode
- A simple animated transition (fade/slide) on a UI element

## Explicitly out of scope
CSS-like styling, a UI editor — see [Phase 3](../phase-3/09-ui-framework.md).
