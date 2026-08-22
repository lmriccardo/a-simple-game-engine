# Scene Management — Phase 3

*Priority: Later — needs a world bigger than one screen to justify it.
See [README](../README.md). Other phases:
[Phase 1](../phase-1/06-scene-management.md) (load/save),
[Phase 2](../phase-2/06-scene-management.md) (prefabs, additive loading).*

## Goals
Stream parts of a large world in/out based on player position, instead of loading
everything up front.

## Design notes
Nothing currently in this engine has a world large enough to need streaming — every
example is a single fixed-size screen. This phase stays unscoped until
[Phase 2](../phase-2/06-scene-management.md)'s additive loading is in real use and a
world actually grows past what fits comfortably in memory at once.

## Tasks
- [ ] Revisit once a real world/level is large enough to need it

## Explicitly out of scope
Everything, until there's a concrete world to stream.
