---
name: run-asge
description: Build, run, and drive ASGE and its example apps (e.g. ecs_demo, texture_demo, moving_box). Use when asked to build ASGE, run an ASGE example, take a screenshot of an ASGE example window, send keyboard input to it, or run its test suite.
---

ASGE examples are native Win32 windows (SDL3 + a software/GDI backend) — there is
no browser or Electron devtools handle to attach to. The handle here is
`.claude/skills/run-asge/driver.ps1`: it launches the exe, waits for its window,
and drives it via the Win32 API (`GetWindowRect`, `SetForegroundWindow`) plus
`System.Windows.Forms.SendKeys` for input and `System.Drawing` for screenshots.

All paths below are relative to the repo root.

## Prerequisites

Windows, CMake, and a Visual Studio 2026 toolchain (the `windows` preset's
generator is `"Visual Studio 18 2026"`). Third-party deps (SDL3, stb) are
vendored via `scripts/install-deps.ps1` on a fresh checkout — not re-verified
this session since they were already present, but that script is the
documented entry point if `cmake --preset windows` fails on a missing dep.

## Build

```powershell
cmake --preset windows
cmake --build --preset windows-debug --target ecs_demo
```

Drop `--target ecs_demo` to build everything (library + every example + every
test binary). Other example binaries follow the same pattern and land at
`bin/Debug/<name>.exe` (`texture_demo`, `moving_box`, `shapes_demo`, ...) — not
individually re-verified this session, only `ecs_demo` was actually driven.

## Run (agent path)

```powershell
powershell -File .claude\skills\run-asge\driver.ps1 launch -Exe bin\Debug\ecs_demo.exe
powershell -File .claude\skills\run-asge\driver.ps1 status
powershell -File .claude\skills\run-asge\driver.ps1 sendkeys -Keys "d"
powershell -File .claude\skills\run-asge\driver.ps1 screenshot -Out "$env:TEMP\asge-shots\ecs_demo.png"
powershell -File .claude\skills\run-asge\driver.ps1 stop
```

Each call after `launch` defaults to the PID `launch` recorded in
`$env:TEMP\asge_driver_pid.txt`; pass `-ProcessId <pid>` explicitly to target a
different one (e.g. driving two windows at once).

| action | flags | what it does |
|---|---|---|
| `launch` | `-Exe <path>` | Starts the exe, waits (up to 10s) for a main window, records its PID |
| `status` | | Prints running/not-running, window title, window handle |
| `sendkeys` | `-Keys "<SendKeys string>"` | Focuses the window, sends keys (e.g. `"w"`, `"{ESC}"`) |
| `screenshot` | `-Out <path.png>` | Captures just that window's rect to a PNG |
| `stop` | | Kills the process, clears the PID file |

Screenshots aren't written anywhere by default — pass an explicit `-Out`, e.g.
under `$env:TEMP\asge-shots\`. **Actually open and look at the PNG** — a blank
or black frame means the app didn't really render.

## Run (human path)

Double-click `bin\Debug\ecs_demo.exe` (or any other example in `bin\Debug\`).
WASD moves the player sprite; close the window to stop. Useless for an agent —
there's no headless mode, the window always opens.

## Test

```powershell
ctest --preset windows-debug
```

541/541 tests passed in ~14.5s (unit + integration suites across ECS, Memory,
Utils, Video, Config, etc.).

## Gotchas

- **Non-ASCII characters break `.ps1` parsing.** A `.ps1` file written without
  a UTF-8 BOM gets read by PowerShell 5.1 under the system codepage, not
  UTF-8. An em dash or curly quote silently corrupts into multi-byte garbage,
  and the parser then throws confusing, unrelated-looking errors *later* in
  the file. `driver.ps1` had two em dashes that broke it exactly this way —
  fixed by sticking to plain ASCII (`-` instead of `—`) throughout.
- **`$pid` is a reserved PowerShell automatic variable** (the *current*
  PowerShell process's own ID) — you cannot assign to it. `driver.ps1` uses
  `$ProcessId`/a resolved local instead; if you're scripting ad hoc, don't
  name a variable `$pid`.
- **Every invocation prints a harmless `Set-PSReadLineOption` warning** from
  this machine's global PowerShell profile (unrelated to ASGE, happens for
  any non-interactive `powershell -File ...` call). Ignore it — the driver's
  real output is the last non-warning line.
- **`stop` deletes the PID file on purpose.** Calling `status`/`sendkeys`/
  `screenshot` afterward without a fresh `launch` (and without `-ProcessId`)
  throws `"No -ProcessId given and no PID file ... - run 'launch' first."`
  with exit code 1 — that's the intended "nothing is tracked" signal, not a
  driver bug.
- **Sprite/texture components need a live `IRenderer`, which doesn't exist at
  construction time** (`IGame`'s constructor runs before the video system
  does). `ecs_demo` and `texture_demo` both defer texture loading — and, for
  `ecs_demo`, attaching `Sprite` components — to the first `Render()` call.

## Troubleshooting

- **`No -ProcessId given and no PID file at ... - run 'launch' first.`**:
  nothing is currently tracked as launched (either you haven't called
  `launch` yet, or you already called `stop`). Run `launch` again.
- **Screenshot is a blank/black window with no sprites**: usually means
  `EnsureSpritesAttached`'s `Image::Load` failed (bad asset path) — check the
  example's `ASGE_..._ASSET_DIR` compile definition in its `CMakeLists.txt`
  actually points at its `assets/` folder.
