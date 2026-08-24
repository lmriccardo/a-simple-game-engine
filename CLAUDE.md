# Project notes for Claude

## Project overview

ASGE ("A Simple Game Engine") is a modular, lightweight, cross-platform 2D
game engine written in modern C++ (C++23), built on top of
[SDL3](https://www.libsdl.org/). It focuses on performance (thin
abstractions over SDL, custom allocators, a thread pool), simplicity (small
composable subsystems rather than one monolith), extensibility (an
ECS-based gameplay layer, a signal/event system for decoupled
communication), and tooling (debug logger, timing/profiling, runnable
examples). Target platforms are Windows, Linux and macOS; rendering
currently goes through SDL3, with other backends (OpenGL/Vulkan/DirectX)
only a future possibility.

Development follows a phased roadmap under `docs/roadmap/` — see
`docs/roadmap/README.md` for the recommended implementation order and
`docs/roadmap/00-overview.md` for the high-level vision. As of the last
update: Core Engine, Rendering Primitives, ECS, and Input are complete;
Asset Pipeline, Scene Management, Physics, Audio and UI are still ahead.

### Repository layout

```
src/ASGE/       Engine source (Application, Core, Events, Game, Input, Video)
examples/       Small standalone programs demonstrating each subsystem
tests/          GoogleTest unit test suite
docs/roadmap/   Design docs and the phased implementation roadmap
scripts/        Dependency-fetching scripts (SDL3, stb) for each platform
third-party/    Vendored dependencies (fetched by the scripts above)
cmake/          CMake package config used when ASGE is installed
```

### Build & test

CMake ≥ 3.31, C++23 compiler. Fetch vendored deps first
(`scripts/install-deps.ps1` / `.sh`), then use the presets in
`CMakePresets.json` (e.g. `cmake --preset windows`, `cmake --build --preset
windows`, `ctest --preset windows-debug`). Full details, plus how to
consume ASGE from another C++ project, are in [README.md](README.md).

## Code documentation

Every function — public API or internal `_internal`/SDL-translation helper —
gets a short doc comment on its **declaration** (the header, not a repeated
one on an out-of-line `.cpp` definition): a Doxygen `/** @brief ... */`, or a
single-line `/** ... */` for something trivial. **4-6 lines max** 
— one or two sentences describing what it does, not an essay. Skip
`@param`/`@return`/`@tparam` breakdowns unless the signature alone doesn't
make them obvious.
