# ECS (Entity Component System) — Phase 2

*Priority: Tier 1 stretch — not MVP-blocking. See [README](../README.md). Other phases:
[Phase 1](../phase-1/03-entity-component-system.md) (done: entities, components, queries).*

## Goals
Address the two things Phase 1 deliberately deferred as performance/structure
concerns rather than correctness gaps: storage layout and system scheduling. Neither
is needed until there's evidence (profiling data, or a system that actually needs
parallelism) that Phase 1's simple approach is the bottleneck.

## Design notes
- **Archetypes/chunked storage** is a data-layout optimization over Phase 1's
  per-type sparse-set store — better cache locality when iterating many components
  together. Only worth the added complexity once
  [13-Optimization](13-optimization.md) profiling actually shows Phase 1's storage is
  a bottleneck; nothing in this engine has enough entities yet for that to be true.
- **Parallel system execution** needs systems to declare what components they
  read/write so a scheduler can find non-conflicting systems to run concurrently —
  meaningless until there are enough systems (physics + AI + animation, say) for
  parallelism to matter. One or two systems (`MovementSystem`, `RenderSystem`) don't
  justify a scheduler.

## Tasks
- [ ] Archetype/chunked storage, gated on profiling evidence from
      [13-Optimization](13-optimization.md)
- [ ] Component read/write declarations per system (prerequisite for parallel execution)
- [ ] Parallel system execution, built on [01-Core Engine](../01-core-engine.md)'s
      existing `ThreadPool`

## Deliverables
- A documented decision on whether archetype storage was adopted, and the profiling
  data that justified it (or a note that it wasn't needed)
- If parallel execution ships: systems annotated with their component access, and a
  scheduler that runs non-conflicting systems concurrently via the existing `ThreadPool`

## Explicitly out of scope
Nothing here is scheduled — this phase only starts once
[Phase 1](../phase-1/03-entity-component-system.md) is in real use and either
profiling or a concrete multi-system workload demands it.
