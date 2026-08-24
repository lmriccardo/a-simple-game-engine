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

## Commit messages

This repo follows **gitmoji + Conventional Commits**:

```
<type>(<scope>): <emoji> <Short imperative summary, no trailing period>

<Optional body: what changed and why, wrapped like prose. Multi-paragraph
is fine for anything non-trivial — this repo's history favours a real
narrative over a bullet list.>
```

- **type**: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`, `ci`, `build`.
- **scope**: lowercase, the subsystem/folder touched — `filesystem`,
  `strings`, `input`, `ecs`, `rendering`, `roadmap`, `skills`, `workflows`,
  `deps`, `structure`. Omit only for a repo-wide `chore` (e.g. a CHANGELOG
  bump).
- **emoji** follows `type`, not the scope:
  - `feat` → `:sparkles:`
  - `fix` → `:bug:`
  - `docs` → `:memo:` (or `:truck:` if it's purely moving/restructuring docs)
  - `test` → `:white_check_mark:`
  - `refactor` → `:recycle:` for behavior-preserving logic changes,
    `:truck:` for a pure rename/move
  - `ci` → `:construction_worker:`
  - `build(deps)` → `:heavy_plus_sign:` for adding a dependency, `:truck:`
    for renaming/relocating one
- **Summary**: capitalized, imperative ("Add", "Fix", "Implement" — not
  "Added"/"Fixes"), short enough to read in a `git log --oneline`.

Examples from this repo's history:
```
feat(filesystem): :sparkles: Implement Virtual File System
fix(strings): :bug: Make StringCRef a genuine const reference
test(filesystem): :white_check_mark: Add VirtualFileSystem unit test suite
docs(roadmap): :memo: Updates Asset pipeline tasks adding Virtual File System Implementation
refactor(structure): :truck: Renames Core/Allocators Folder into Core/Memory to fit more datastructures inside of it
ci(workflows): :construction_worker: Adds concurrency checks for duplicated workflows
```

Merge commits and automated `chore: update CHANGELOG.md for vX.Y.Z (#N)`
commits are machine-generated and don't follow this pattern — don't imitate
them for a hand-written commit.
