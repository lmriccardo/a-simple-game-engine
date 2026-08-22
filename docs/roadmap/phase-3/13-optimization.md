# Optimization — Phase 3

*Priority: Tier 4, but continuous in spirit — this phase specifically waits for
real content. See [README](../README.md). Other phases:
[Phase 1](../phase-1/13-optimization.md) (CPU profiling — done),
[Phase 2](../phase-2/13-optimization.md) (GPU profiling, job system).*

## Goals
Streaming optimizations and ECS storage/cache optimization — a dedicated deep
performance pass, only once there's enough real content to profile.

## Design notes
This is deliberately the last phase, not because it's low priority in the
abstract, but because it needs data that doesn't exist yet: real profiling numbers
from [Phase 1](../phase-1/13-optimization.md)'s `TimingProfiler` on an actual
game's content, not a synthetic benchmark. ECS cache optimization specifically is
[03-ECS Phase 2](../phase-2/03-entity-component-system.md)'s archetype/chunked
storage — this doc is the "why now" trigger for that work, not a separate task.

## Tasks
- [ ] Profile real content once it exists; let the data pick what to optimize
      rather than guessing
- [ ] If `TimingProfiler`/GPU profiling data shows ECS storage is the bottleneck,
      trigger [03-ECS Phase 2](../phase-2/03-entity-component-system.md)'s
      archetype storage
- [ ] Streaming optimizations, once
      [06-Scene Management Phase 3](../phase-3/06-scene-management.md)'s world
      streaming exists to optimize

## Explicitly out of scope
Optimizing anything without profiling data justifying it first — that's the entire
point of deferring this phase.
