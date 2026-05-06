# Core Engine

## Goals
Build the foundational runtime.

## Tasks
- [x] Create application layer
- [x] Window management
- [x] Main loop
- [x] Timing system
- [ ] Threading utilities
- [ ] Logging system
- [ ] Memory allocator
- [ ] Config system

#### Timing System

The `TimingSystem` tracks times between frames and provides consistent time values to the rest of the engine.
Main responsabilities are:

- Measure `deltaTime`
- Track total engine uptime
- Support fixed timestep updates
- Support frame limiting
- Support pause/resume
- Provide high-resolution timers
- Separate real time from game time

## Milestones
### Phase 1
- Window opens
- Engine loop runs
- Delta time works

### Phase 2
- Logging + profiler
- Thread-safe systems
- Hot reload support

## Deliverables
- Stable runtime
- Modular architecture