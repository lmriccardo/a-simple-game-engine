# Scripting & Events — Phase 1

*Priority: Tier 3 — worth doing once enough ECS systems exist that they need
decoupling from each other. See [README](../README.md).
Other phases: [Phase 2](../phase-2/11-scripting-and-events.md) (managed scripting,
runtime reload).*

## Goals
An event dispatcher gameplay systems can use to react to each other without direct
coupling, and native (C++) scripting hooks for entity behavior.

## Current state
**The event-dispatcher primitive this needs already exists**, built for a different
purpose: `asge::signals::Signal<Args...>`
([Signal.hpp](../../../src/ASGE/Core/Patterns/Signal.hpp)) is a generic, thread-safe,
type-safe signal/slot implementation — `Connect`/`Emit`/automatic disconnection via
lifetime tokens — already used in production by
[01-Core Engine](../01-core-engine.md)'s `FileWatcher` for change notifications.
What's missing isn't the dispatch primitive, it's a game-event-specific bus built on
top of it (named/typed gameplay events systems can publish and subscribe to). No
scripting/behavior code exists at all yet;
[03-ECS Phase 1](../phase-1/03-entity-component-system.md) stubs a `Script`
component specifically for this phase to fill in.

## Design notes
- **Build the event bus on `Signal`, don't reinvent dispatch.** A `Signal<Args...>`
  per event type (or a type-erased registry of them keyed by event type) gives
  gameplay systems pub/sub without writing a new dispatch mechanism — `Signal`
  already solves the hard parts (thread safety, automatic disconnection).
- **Native scripting means a `Script` component invoking a plain C++
  function/functor**, not a bytecode VM or reflection system — the
  [03-ECS](03-entity-component-system.md) `Script` placeholder becomes a real
  callback hook, not a new language.
- **Managed scripting (Lua/C#/Python) is well past MVP** and stays entirely in
  [Phase 2](../phase-2/11-scripting-and-events.md) — it needs a real embedding +
  binding-generation story that doesn't exist yet and shouldn't be guessed at here.

## Tasks
- [ ] A game-event bus built on `asge::signals::Signal`: named/typed events,
      publish/subscribe from gameplay systems
- [ ] `Script` component: wraps a native C++ callback invoked per-frame or on a
      specific event
- [ ] At least one ECS system decoupled from another via the event bus instead of
      a direct function call, proving it's actually useful

## Deliverables
- An event bus usable from at least two independent gameplay systems
- `Script` component wired into the ECS, invoked from an example

## Explicitly out of scope for Phase 1
Managed scripting (Lua/C#/Python), runtime reload — see
[Phase 2](../phase-2/11-scripting-and-events.md). A reflection system — not needed
for native callbacks, and [06-Scene Management](06-scene-management.md) is the doc
that will define real serialization requirements if one's ever needed.
