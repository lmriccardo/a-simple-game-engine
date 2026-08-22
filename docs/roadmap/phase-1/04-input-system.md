# Input System — Phase 1

*Priority: Tier 1 — after ECS. See [README](../README.md). Other phases:
[Phase 2](../phase-2/04-input-system.md) (gamepad, remapping),
[Phase 3](../phase-3/04-input-system.md) (touch).*

## Goals
Support flexible user input. Scoped for Phase 1 to finishing what keyboard input
already started: mouse events flowing through the same `SystemEvent` pipeline, plus a
queryable input state so games stop hand-rolling their own key-state bookkeeping.

## Current state
- **Keyboard is fully wired**: SDL key up/down → `EventType::KEYBOARD_KEY_PRESSED` /
  `_RELEASED` → `event::KeyboardEvent` (window id, keyboard id, `Keycode`, `Keymod`,
  down, repeat) → `SystemEvent` → `Application::processEvent` →
  `IGame::OnSystemEvent`. Used today in `background_changer`, `ecs_demo`,
  `moving_box`/`MovingBoxGame`, `input_demo` (stubbed but unused in `shapes_demo`,
  `texture_demo`, `text_demo`).
- **Mouse is fully wired**, same pipeline: `EventType::MOUSE_MOTION` /
  `MOUSE_BUTTON_PRESSED` / `_RELEASED` / `MOUSE_WHEEL_MOTION` →
  `MouseMotionEvent`/`MouseButtonEvent`/`MouseWheelEvent`.
- **`Keycode` is dense** (`UNKNOWN=0, ESCAPE, SPACE, A..Z, COUNT`), deliberately not
  value-compatible with SDL's ASCII-derived `SDLK_*` constants, so it doubles as an
  array index. Translated via explicit `ToKeycode`/`ToSdlKeycode` mappings in
  `SDL_TranslationUnit`, not a bare `static_cast`. Only covers `ESCAPE`, `SPACE`,
  `A`-`Z` — no digits, arrows, or function keys yet; widen on demand.
- **`InputState`/`InputSystem` provide queryable polling**: `IsKeyDown`/`Pressed`/
  `Released`, matching mouse-button queries, `GetMousePosition`/`GetMouseDelta`/
  `GetScrollDelta`. `InputSystem` (shaped like `VideoSystem`) is owned by
  `Application`, fed every frame, and threaded into `IGame::Update`'s second
  parameter.
- **`examples/input_demo`** exercises every `InputState` query in a real, runtime-
  verified loop with `OnSystemEvent` left empty — the integration proof that polling
  alone is enough for a real game.
- **No controller/gamepad, rebinding, or action-map code exists at all** — no
  `Enums.hpp` entries, no SDL gamepad translation, no examples. Entirely Phase 2/3
  work (see sibling docs above).

## Design notes
- **Event-driven core, polling convenience layer on top.** `SystemEvent` stays the
  single source of truth (matches [01-Core Engine](../01-core-engine.md)'s existing
  `Application` loop); `InputState`/`InputSystem` are a thin accumulator fed by that
  same stream each frame, not a second, independent polling backend. Games can still
  read raw events from `OnSystemEvent` when they need edge-triggered behavior driven
  directly off the transport (e.g. the background-changer's space-bar toggle).
- **Mouse events are three distinct payloads, not one blob.** Motion (position +
  delta), button (which button + up/down), and wheel (scroll delta) carry different
  data — modeled as three small structs in the `_SystemEvent` variant, following
  `KeyboardEvent`'s shape (window id + `CommonEvent` timestamp/type base).
- **Keycode table grows on demand, not preemptively.** Widen `input::Keycode`
  (arrows, digits, function keys) only as an example or the action-map layer
  actually needs a key that's missing.
- **`InputState` caches, it doesn't replace, `Render()`'s inability to see input** —
  `IGame::Render` only gets an `IRenderer`, so anything drawn from input state (e.g.
  a mouse cursor) has to be cached into a member during `Update()` first, same as
  `input_demo` does.

## Tasks
- [x] Mouse `SystemEventType` + `EventType` classification wiring
- [x] `MouseMotionEvent` / `MouseButtonEvent` / `MouseWheelEvent` structs + variant entry
- [x] SDL mouse translation (`__processMouse`, `GetProcessMap` entry)
- [x] Dense `Keycode` + explicit `ToKeycode`/`ToSdlKeycode` mapping
- [x] `ASGE/Input` module (`Keycode.hpp`/`MouseButton.hpp` moved out of `Events/`)
- [x] Queryable `InputState`: `IsKeyDown`/`IsKeyPressed`/`IsKeyReleased`,
      `IsMouseButtonDown`/`Pressed`/`Released`, `GetMousePosition`/`GetMouseDelta`/
      `GetScrollDelta`
- [x] `InputSystem` owned by `Application`, wired into `IGame::Update`
- [x] Integration example (`input_demo`) proving polling replaces manual bookkeeping
- [ ] Widen `Keycode` with arrows + digits (first consumers likely need them)

## Milestones

### Step 1 — Mouse event plumbing

*Status:* ✅ **Done**

- `SystemEventType::MOUSE`, the three mouse event structs, SDL translation, and
  registration in `GetProcessMap`
- Mouse position reported in window coordinates (`math::Float2`), matching the
  coordinate space `DrawTexture`/`DrawRect` already use
  ([02-Rendering Primitives](../02-rendering-primitives.md))
- Unit tests: `tests/unit/Events/MouseEventTranslationTests.cpp`
- Also reworked `Keycode` to be dense instead of value-matching SDL's `SDLK_*`
  constants, with an explicit `ToKeycode`/`ToSdlKeycode` mapping — makes `Keycode`
  usable as an array index without depending on SDL's ASCII-derived values. Moved
  `Keycode.hpp`/`MouseButton.hpp` into a new `ASGE/Input/` module — they're
  `asge::input` vocabulary types, not part of the event-transport pipeline.

### Step 2 — Queryable input state

*Status:* ✅ **Done**

- `InputState` fed via `Consume(SystemEvent const&)` from the same stream
  `Application::Run` already pumps, plus `NewFrame()` to roll current state into
  previous for edge detection
- Backed by `std::array<bool, Keycode::COUNT>` / `std::array<bool, MouseButton::COUNT>`
  current+previous frame snapshots — cheap, fixed-size, no allocation
- `InputSystem` owns the `InputState` and drives it, shaped like `VideoSystem`
  (`NewFrame()` + `ProcessEvent()` + `GetState()`) — the piece `Application` actually
  holds
- Unit tests: `tests/unit/Input/InputStateTests.cpp`, `InputSystemTests.cpp`

### Step 3 — Integration demo

*Status:* ✅ **Done**

- `Application` owns an `InputSystem`; `Run()` calls `NewFrame()`/`ProcessEvent()`
  each loop iteration; `IGame::Update` grew an `InputState const&` parameter passed
  from `GetState()`. All six pre-existing examples updated to match.
- New `examples/input_demo`: `OnSystemEvent` deliberately empty, every reaction to
  input comes from polling `InputState` in `Update()` — `IsKeyDown` (continuous WASD
  movement), `IsKeyPressed` (edge-triggered SPACE toggle), `GetMousePosition` +
  `IsMouseButtonDown` (a cursor that fills while held), `IsMouseButtonPressed`
  (edge-triggered right-click marks), `GetScrollDelta` (mouse-wheel resize)
- Runtime-verified via the `run-asge` skill (`SendKeys` for keyboard, an ad hoc
  `SetCursorPos`/`mouse_event` probe for mouse) with screenshots confirming every
  behavior actually happens on screen

## Deliverables
- Mouse events flowing end-to-end through the existing `SystemEvent`/`IGame` pipeline
- `InputState` polling API covering keyboard + mouse
- At least one example driven by `InputState` instead of manual event bookkeeping
  (`examples/input_demo`)
- Unit tests for event translation and input-state transitions

## Explicitly out of scope for Phase 1
Controller/gamepad support ([Phase 2](../phase-2/04-input-system.md)), input
rebinding and action maps (also Phase 2, blocked on gamepad giving the engine a
second scheme to map against), and touch input
([Phase 3](../phase-3/04-input-system.md)).
