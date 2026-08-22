# Input System

*Priority: Tier 1 — after ECS. See [README](README.md).*

## Goals
Support flexible user input. Scoped for Phase 1 to finishing what keyboard input
already started: mouse events flowing through the same `SystemEvent` pipeline, plus a
queryable input state so games stop hand-rolling their own key-state bookkeeping.

## Current state
- **Keyboard is fully wired**: SDL key up/down → `EventType::KEYBOARD_KEY_PRESSED` /
  `_RELEASED` → `event::KeyboardEvent` (window id, keyboard id, `Keycode`, `Keymod`,
  down, repeat) → `SystemEvent` → `Application::processEvent` →
  `IGame::OnSystemEvent`. Used today in `background_changer`, `ecs_demo`,
  `moving_box`/`MovingBoxGame` (stubbed but unused in `shapes_demo`, `texture_demo`,
  `text_demo`).
- **`Keycode` only covers a small subset**: `UNKNOWN`, `ESCAPE`, `SPACE`, `A`-`Z`. No
  digits, arrows, function keys, or modifier-as-key entries yet — every example so far
  only needed WASD + Space, so the table was never widened past that.
- **Mouse is declared but not implemented.** `EventType` already has `MOUSE_MOTION`,
  `MOUSE_BUTTON_PRESSED`, `MOUSE_BUTTON_RELEASED`, `MOUSE_WHEEL_MOTION`
  ([Enums.hpp](../../src/ASGE/Events/Enums.hpp)) and the offset range for them, but the
  wiring stops there:
  - `SystemEventType` has no `MOUSE` variant, so `IsValidSysType`/`ToSysEventTypeImpl`
    can never classify a mouse `EventType` as anything but `UKNOWN`.
  - `Events.hpp`'s `_SystemEvent` variant and `IsSystemEvent` concept have no mouse
    event struct at all.
  - `SDL_TranslationUnit`'s `ToEventType` doesn't map `SDL_EVENT_MOUSE_MOTION` /
    `MOUSE_BUTTON_DOWN` / `MOUSE_BUTTON_UP` / `MOUSE_WHEEL`, there's no
    `__processMouse`, and `GetProcessMap` has no `MOUSE` entry.
  - Net effect: an SDL mouse event today produces `EventType::UKNOWN` and is silently
    dropped by `Application::processEvent` before it ever reaches a game.
- **No queryable input state.** Everything is push-only through `OnSystemEvent`. Every
  example that needs held-key state (`ecs_demo`, `moving_box`) hand-rolls its own
  `m_Up`/`m_Down`/`m_Left`/`m_Right` booleans updated from `KeyboardEvent`s — the same
  four-line switch duplicated in `EcsDemoGame::OnSystemEvent` and
  `MovingBox::OnKeyboardEvent`. That's exactly the kind of bookkeeping a queryable
  `InputState` (`IsKeyDown(Keycode)`, `IsMouseButtonDown`, `MousePosition()`) removes.
- **No controller/gamepad, rebinding, or action-map code exists at all** — no `Enums.hpp`
  entries, no SDL gamepad translation, no examples. Entirely Phase 2/3 work.

## Design notes
- **Event-driven core, polling convenience layer on top.** `SystemEvent` stays the
  single source of truth (matches [01-Core Engine](01-core-engine.md)'s existing
  `Application` loop); an `InputState`/`InputManager` is a thin accumulator fed by that
  same stream each frame, not a second, independent polling backend. Games can still
  read raw events from `OnSystemEvent` when they need edge-triggered behavior (e.g.
  the background-changer's space-bar toggle).
- **Mouse events are three distinct payloads, not one blob.** Motion (position + delta),
  button (which button + up/down), and wheel (scroll delta) carry different data — model
  them the way `KeyboardEvent` models keyboard, as one or more small structs in the
  `_SystemEvent` variant, following `KeyboardEvent`'s existing shape (window id +
  `CommonEvent` timestamp/type base).
- **Keycode table grows on demand, not preemptively.** Widen `input::Keycode` (arrows,
  digits, function keys) only as an example or the action-map layer actually needs a
  key that's missing, same as it was built so far.
- **Gamepad support is a separate SDL subsystem** (`SDL_gamepad.h`, already vendored
  under `third-party/SDL`) with its own connect/disconnect lifecycle — not an extension
  of the keyboard/mouse event shapes. Deferred to Phase 2 so Phase 1 stays scoped to
  what an MVP 2D game actually needs first.
- **No rebinding/action-map system until there's a second control scheme to map.**
  Building a generic action-map abstraction against a single hardcoded WASD scheme
  would be guessing at requirements; do it once gamepad support (Phase 2) gives the
  engine two physical input sources that plausibly need to map to the same logical
  action.

## Tasks
- [x] Mouse `SystemEventType` + `EventType` classification wiring (`Enums.hpp`)
- [x] `MouseMotionEvent` / `MouseButtonEvent` / `MouseWheelEvent` structs + variant entry
      (`Events.hpp`)
- [x] SDL mouse translation (`SDL_TranslationUnit`): `ToEventType` mapping,
      `__processMouse`, `GetProcessMap` entry
- [x] Queryable `InputState`: `IsKeyDown`/`IsKeyPressed`/`IsKeyReleased`,
      `IsMouseButtonDown`/`Pressed`/`Released`, `GetMousePosition`/`GetMouseDelta`/
      `GetScrollDelta`
- [ ] Widen `Keycode` with arrows + digits (first consumers likely need them)
- [ ] Controller support — Phase 2
- [ ] Input rebinding — Phase 2/3, blocked on having two schemes to map between
- [ ] Action maps — Phase 2/3, same blocker
- [ ] Touch input — Phase 3

## Milestones

### Step 1 — Mouse event plumbing

*Status:* ✅ **Done**

- Filled the gap described in **Current state**: `SystemEventType::MOUSE`
  ([Enums.hpp](../../src/ASGE/Events/Enums.hpp)), `MouseMotionEvent` /
  `MouseButtonEvent` / `MouseWheelEvent` in the `SystemEvent` variant
  ([Events.hpp](../../src/ASGE/Events/Events.hpp)), a new `input::MouseButton` enum
  ([MouseButton.hpp](../../src/ASGE/Input/MouseButton.hpp)) mirroring `Keycode`'s
  shape, and SDL translation (`__processMouse` + `GetProcessMap` entry in
  [SDL_TranslationUnit.cpp](../../src/ASGE/Events/Translation/SDL_TranslationUnit.cpp))
- Mouse position reported in window coordinates (`math::Float2`), matching the
  coordinate space `DrawTexture`/`DrawRect` already use
  ([02-Rendering Primitives](02-rendering-primitives.md))
- Unit tests: `tests/unit/Events/MouseEventTranslationTests.cpp` — SDL mouse event →
  correct `EventType`/struct fields for motion, button down/up, and wheel, plus a
  guard that an unrelated event (quit) doesn't get misclassified as a mouse event.
  First dedicated event-translation test suite.
- Also reworked `Keycode` to be dense (`UNKNOWN=0, ESCAPE, SPACE, A..Z, COUNT`) instead
  of value-matching SDL's `SDLK_*` constants, with an explicit `ToKeycode`/`ToSdlKeycode`
  mapping in `SDL_TranslationUnit` — makes `Keycode` usable as an array index (needed by
  Step 2's per-key state table) without depending on SDL's ASCII-derived values.
- Moved `Keycode.hpp`/`MouseButton.hpp` out of `Events/` into a new `ASGE/Input/` module
  — they're `asge::input` vocabulary types, not part of the event-transport pipeline.

### Step 2 — Queryable input state

*Status:* ✅ **Done**

- `InputState` ([InputState.hpp](../../src/ASGE/Input/InputState.hpp)) fed via
  `Consume(SystemEvent const&)` from the same stream `Application::Run` already pumps —
  not a second event source — plus `NewFrame()` to roll current state into previous for
  edge detection
- `IsKeyDown`/`IsKeyPressed`(edge)/`IsKeyReleased`(edge), matching
  `IsMouseButtonDown`/`Pressed`/`Released`, and `GetMousePosition`/`GetMouseDelta`
  (since last `NewFrame()`)/`GetScrollDelta` (accumulated since last `NewFrame()`)
- Backed by `std::array<bool, Keycode::COUNT>` / `std::array<bool, MouseButton::COUNT>`
  current+previous frame snapshots — cheap, fixed-size, no allocation
- Unit tests: `tests/unit/Input/InputStateTests.cpp` — down/pressed/released transitions
  for keys and mouse buttons, mouse position/delta across `NewFrame()` boundaries, scroll
  accumulation + reset, and unrelated events (quit) are ignored without side effects

### Step 3 — Integration demo

*Status:* 🟡 **Plumbing wired, migration not done yet**

- `InputSystem` ([InputSystem.hpp](../../src/ASGE/Input/InputSystem.hpp)) is the piece
  `Application` actually owns — shaped like `VideoSystem`: `NewFrame()` once per loop
  iteration, `ProcessEvent` for every polled `SystemEvent`, `GetState()` for read access.
  `Application::Run` now calls both and passes `GetState()` into `IGame::Update`, whose
  signature grew an `input::InputState const&` parameter — every example (`ecs_demo`,
  `moving_box`, `background_changer`, `shapes_demo`, `text_demo`, `texture_demo`) updated
  to match and confirmed still building/running (`ecs_demo` smoke-tested via the
  `run-asge` skill: launches, accepts WASD, renders).
- **Not done yet**: `ecs_demo`'s player movement still hand-tracks
  `m_Up`/`m_Down`/`m_Left`/`m_Right` via `OnSystemEvent` — it has the `InputState`
  parameter available in `Update` now but doesn't read from it. Migrating that (and
  exercising a mouse event somewhere real, since nothing does today) is what's left to
  actually close this step.

### Phase 2 — stretch, not MVP-blocking

- Gamepad support via `SDL_gamepad.h` (connect/disconnect, button + axis events)
- Input remapping — once gamepad gives the engine a second scheme to map against
  keyboard/mouse

### Phase 3 — later

- Touch input

## Deliverables
- Mouse events flowing end-to-end through the existing `SystemEvent`/`IGame` pipeline,
  closing the gap called out in the README reorder
- `InputState` polling API covering keyboard + mouse
- At least one example driven by `InputState` instead of manual event bookkeeping
- Unit tests for event translation and input-state transitions

## Explicitly out of scope for Phase 1
Controller/gamepad support, input rebinding, action maps, and touch input. Each is
either a Phase 2 addition with its own SDL subsystem (gamepad) or blocked on gamepad
existing first (rebinding/action maps need two schemes to map between before the
abstraction means anything), or a Phase 3 concern (touch) with no target platform in
[00-overview](00-overview.md) that needs it yet.
