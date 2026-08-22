# Physics System — Phase 3

*Priority: Later — specialized gameplay, no current use case.
See [README](../README.md). Other phases:
[Phase 1](../phase-1/07-physics-system.md) (AABB, gravity, collision response),
[Phase 2](../phase-2/07-physics-system.md) (CCD, raycasts, character controller).*

## Goals
Soft body and vehicle physics, for whenever a specific gameplay feature actually
needs either.

## Design notes
Both are specialized simulation modes with real complexity cost (constraint
solvers, wheel/suspension models) that no example or planned feature currently
needs. This stays unscoped until a concrete gameplay feature requires one of them —
sketching an API now would be guessing.

## Tasks
- [ ] Revisit once a concrete feature needs soft body or vehicle simulation

## Explicitly out of scope
Everything, until a concrete feature demands it.
