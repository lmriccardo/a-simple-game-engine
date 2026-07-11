# Core Engine

## Goals
Build the foundational runtime.

## Tasks
- [x] Create application layer
- [x] Window management
- [x] Main loop
- [x] Timing system
- [x] Threading utilities
- [x] Logging system
- [x] Memory allocator
- [WIP] Config system
    1. TOML support parser and value provider
    2. Generic configuration manager
    3. FileWatcher + generic filesystem utilities (cross-platform)
    4. Hot-Reload support into the configuration manager
    5. Configuration Writing back to the file
- [x] Error system

## Milestones

### Phase 1

*Status:* ✅ **COMPLETED**

- Window opens
- Engine loop runs
- Delta time works

### Phase 2

*Status* : 🚧 **WORK IN PROGRESS**

- Logging + profiler
- Thread-safe systems
- Hot reload support

## Deliverables
- Stable runtime
- Modular architecture