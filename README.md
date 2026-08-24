# ASGE — A Simple Game Engine

ASGE is a modular, lightweight, cross-platform 2D game engine written in
modern C++ (C++23), built on top of [SDL3](https://www.libsdl.org/). It's
developed with a focus on:

- **Performance** — thin abstractions over SDL, custom allocators, a
  thread pool for concurrent work.
- **Simplicity** — small, composable subsystems instead of one monolithic
  framework.
- **Extensibility** — an ECS-based gameplay layer and a signal/event system
  for decoupled communication between systems.
- **Tooling** — a debug logger, timing/profiling utilities, and a growing
  set of runnable examples.

The engine targets Windows, Linux and macOS, and currently renders through
SDL3, with OpenGL/Vulkan/DirectX backends being considered for the future.

## Current status

ASGE is under active development, worked through a
[roadmap](docs/roadmap/README.md) split into phases. As of now:

- ✅ Core engine (application loop, logging, timing, math, memory,
  concurrency, filesystem)
- ✅ Rendering primitives (shapes, textures, text)
- ✅ Entity-Component-System (ECS)
- ✅ Input system (keyboard, mouse)
- 🚧 Asset pipeline, scene management, physics, audio, UI, and more —
  see [docs/roadmap](docs/roadmap/README.md) for the full plan.

## Repository layout

```
src/ASGE/       Engine source (Application, Core, Events, Game, Input, Video)
examples/       Small standalone programs demonstrating each subsystem
tests/          GoogleTest unit test suite
docs/roadmap/   Design docs and the phased implementation roadmap
scripts/        Dependency-fetching scripts (SDL3, stb) for each platform
third-party/    Vendored dependencies (fetched by the scripts above)
cmake/          CMake package config used when ASGE is installed
```

## Installation

### Prerequisites

- A C++23 compiler (MSVC on Windows, GCC/Clang on Linux, Clang on macOS)
- [CMake](https://cmake.org/) ≥ 3.31
- Git

### Building from source

```bash
git clone https://github.com/lmriccardo/a-simple-game-engine.git asge
cd asge
```

Fetch the vendored dependencies (SDL3 and the stb single-header libraries)
for your platform:

```powershell
# Windows (PowerShell)
./scripts/install-deps.ps1
```

```bash
# Linux / macOS
./scripts/install-deps.sh
```

Then configure and build using one of the provided
[CMake presets](CMakePresets.json) (also buildable without a preset, via
plain `cmake -S . -B build`):

```bash
cmake --preset windows      # or: linux / macos
cmake --build --preset windows
ctest --preset windows-debug
```

Built examples land in `bin/`.

## Using ASGE in your own C++ project

ASGE builds a static library target and installs a CMake package config, so
it can be consumed either as a subdirectory or as an installed package.

### Option 1 — `add_subdirectory`

Vendor ASGE into your project (e.g. as a submodule under `third-party/asge`)
and add it directly:

```cmake
add_subdirectory(third-party/asge)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE ASGE)
```

On Windows, `my_game.exe` also needs `SDL3.dll` next to it at runtime. Since
`add_subdirectory` never exposes ASGE's own SDL3 target back up to your
project's directory scope, use the helper ASGE defines for exactly this
instead of reaching for the SDL3 target directly:

```cmake
asge_copy_sdl3_runtime(my_game)
```

### Option 2 — installed package

Install ASGE (`cmake --install build`), then from your own project:

```cmake
find_package(ASGE REQUIRED CONFIG)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE ASGE::ASGE)
```

Either way, pull in the whole engine through the umbrella header:

```cpp
#include <ASGE/ASGE.hpp>
```

## Example

A minimal game that just opens a window and runs the game loop
(see [examples/moving_box](examples/moving_box) for a complete, playable
example with input and rendering):

```cpp
#include <ASGE/ASGE.hpp>

class MyGame : public asge::game::IGame
{
public:
    void Update(float inDeltaTime, asge::input::InputState const& inInput) override
    {
        // Game logic goes here
    }

    void Render(asge::video::IRenderer& inRenderer) override
    {
        // Draw calls go here
    }

    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override
    {
        // Handle window/system events
    }
};

int main()
{
    MyGame game;
    asge::Application app(game, asge::ApplicationConfig{});
    app.Run();
    return 0;
}
```

More examples covering shapes, textures, text, ECS, input handling and
configuration loading live under [examples/](examples/) and are built
alongside the engine when `ASGE_BUILD_EXAMPLES` is `ON`. In particular, for
drawing a texture and reading keyboard/mouse input — the two things a
real game needs beyond the empty overrides above — see
[examples/texture_demo](examples/texture_demo) (`IRenderer::DrawTexture`)
and [examples/moving_box](examples/moving_box)
(`InputState::IsKeyDown`/`IsKeyPressed`).

## Testing

Unit tests (GoogleTest) live under [tests/](tests/) and build when
`BUILD_TESTING` is `ON`:

```bash
ctest --preset windows-debug   # or the linux / macos equivalent
```

## Contributing

Contributions are welcome — please open an issue or pull request. See
[docs/roadmap](docs/roadmap/README.md) for the current priorities and
`CLAUDE.md` for this repository's commit message and documentation
conventions.

## Contributors

- [Riccardo La Marca](https://github.com/lmriccardo) — maintainer
- [Davidizzle](https://github.com/davidizzle)
