# UI Framework — Phase 3

*Priority: Later — tooling investment, not a shipping feature.
See [README](../README.md). Other phases:
[Phase 1](../phase-1/09-ui-framework.md) (immediate-mode, debug overlays),
[Phase 2](../phase-2/09-ui-framework.md) (retained-mode UI, animation).*

## Goals
A declarative styling layer for retained-mode UI, and a visual editor for building
UI without hand-writing layout code.

## Design notes
Both are significant tooling investments that only pay off once
[Phase 2](../phase-2/09-ui-framework.md)'s retained-mode UI is in real use across
multiple screens/menus with enough repeated styling to make a stylesheet-like layer
worth it. Stays unscoped until then.

## Tasks
- [ ] Revisit once retained-mode UI is used widely enough to need shared styling
      or visual authoring

## Explicitly out of scope
Everything, until Phase 2 is in wide use.
