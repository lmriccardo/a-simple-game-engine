# ASGE Level Editor — Build Roadmap

Progressive implementation plan for an in-engine, Dear ImGui–based level
editor for ASGE. Each phase is a working, testable milestone — no phase
depends on unbuilt infrastructure from a later one. Follows ASGE's existing
principles: no speculative abstraction, prove the narrow case before
generalizing, single source of truth for serialization.

**Target shape:** a separate executable (`asge-editor`) linking `Core/`
directly (ECS, Video, Math, TOML, Filesystem) — no dependency on `Game/` or
`IGameState`. The editor edits a real `Registry` and saves through the exact
same `Serializer<T>` / `SceneManager` path the game uses. There is no
parallel schema, no parallel renderer.

---

## Phase 0 — Vendor & wire Dear ImGui

**Goal:** ImGui source present and building, no editor logic yet.

- [x] Vendored Dear ImGui (core + SDL3/SDLRenderer3 backends) alongside the
      other vendored libs (`stb_image`, `stb_truetype`, TOML parser).
- [x] New `asge-editor` CMake target, linking `Core` only — not `Game`.
- [x] Added an `IRenderer` accessor for the raw `SDL_Renderer*` the ImGui
      SDL3 backend needs directly.
- [x] Minimal `main()`: SDL3 + ImGui init, event loop rendering the ImGui
      demo window.

**Done when:** empty engine window with ImGui's demo window rendering on
top, no crashes on close.

---

## Phase 1 — Load a real scene into the editor

**Goal:** prove the editor operates on genuine engine state, not a mock.

- [x] Loads a real `.toml` scene via `SceneManager::Load` into a live
      `Registry` (hardcoded test path for now).
- [x] Runs the existing, unmodified render pipeline each frame.
- [x] Single "Scene loaded: N entities" ImGui text window.

**Done when:** a real authored `.toml` scene renders correctly inside the
editor executable, using the unmodified render pipeline.

---

## Phase 2 — Prove the round trip on one component: `Transform`

**Goal:** confirm select → edit → save → reload works end to end before
building any generalized machinery.

- [x] Screen→world picking via the renderer's own camera/viewport math.
- [x] Click-to-select against an entity's `Transform`/sprite bounds.
- [ ] Viewport drag-to-move — skipped; `DragFloat2` already covered editing.
- [x] Inspector `DragFloat2` mutating the live `Transform` directly, no copy.
- [x] Save goes through the unmodified `Serializer<Transform>` path.
- [x] Verified manually: edit position → save → reload in the real game
      shows the new position.

**Done when:** that manual verification passes — proves there's no schema
duplication.

---

## Phase 3 — Generalize the inspector to more component types

- [x] One `DrawInspector(T&)` free function per serializable component type
      — mirrors the `Serializer<T>` convention, no reflection system.
- [x] Entity list panel via the same fixed per-type `HasComponent` checks.
- [x] Selecting from the list drives the same inspector as viewport picking.

**Done when:** every currently-serializable component type is editable on
any selected entity, round-tripping through Phase 2's save path.

---

## Phase 4 — Visual collider overlays & basic gizmos

- [x] World-space coordinate grid overlay, panning/zooming with the camera.
- [x] `Collider` bounds drawn as translucent overlays, colored by layer.
- [x] Viewport free-drag to reposition a selected entity, unconstrained.
- [x] Translate gizmo with X/Y arrow handles for axis-locked movement.

**Done when:** collider placement/overlap and world position are readable
directly off the viewport, without switching to the running game.

---

## Phase 5 — Entity lifecycle: create, delete, duplicate

- [x] Create entity via the same `Registry::Create` + component path
      gameplay code uses, tagged with `SceneId` so Save doesn't drop it.
- [x] Delete selected entity.
- [x] Duplicate selected entity, copying every present component.
- [x] File > New resets to a genuinely empty scene.
- [x] File > Save As prompts for a destination and retags `SceneId`.

**Done when:** a level can be authored from an empty scene and saved to a
new file of its own.

---

## Phase 6 — Editor polish: component editing, native dialogs, diagnostics

- [x] Add/remove components on an existing entity from the Inspector, via a
      shared type-erased `Has`/`Add`/`Remove` table.
- [x] No scene auto-loaded on startup — opens empty, like File > New.
- [x] Save/Save As/Open go through native OS file dialogs.
- [x] Editor window/taskbar icon set from the project logo.
- [x] In-editor log console wired to `Logger::OnLog` (fixed a real engine
      bug along the way: `FormatTimestamp` ignored its own argument).
- [x] Log panel anchored to the window edges, with error/warning badges and
      a "Save Log" button.

**Done when:** every entity's component makeup is editable from the
Inspector alone, no invisible scratch save path, and every log message is
visible on screen.

---

## Phase 7 — Asset awareness

- [x] Assets panel lists known texture/animation paths, derived from
      registry usage (not a filesystem scan — tried and discarded two
      scan-based designs first).
- [x] "Load Asset..." imports a path via a mount picker + native dialog,
      for assets an empty scene has no entity to derive from yet.
- [x] Scene load registers its asset paths so they persist independent of
      whether anything currently references them.
- [x] Clicking an entry opens an Asset Inspector (path, size, dimensions,
      thumbnail preview).
- [x] Sprite Add-Component requires picking a known texture up front rather
      than starting blank.
- [x] Always-on VFS panel: lists/adds mounts, flags any root the current
      scene needs that isn't mounted.
- [x] Re-mounting an already-bound name replaces it instead of duplicating.
- [x] No mount is ever auto-created; Open resolves its file via a private
      root that's unmounted again immediately after.

**Done when:** authoring a new entity's visuals doesn't require knowing
asset filenames by memory.

---

## Phase 8 — Editor session persistence (`.asges`)

**Goal:** reopening the editor on a project already being worked on
restores mounts, known assets, and the active scene in one step.

- [x] `.asges` format: plain TOML through the existing `asge::toml`
      machinery — mounts, known texture/animation paths, current scene path.
- [x] "Save Session..." / "Open Session..." in the File menu, via the same
      native-dialog plumbing as Save/Open Scene.
- [x] Save writes every current mount, known asset path, and the active
      scene path.
- [x] Open replaces the whole working state outright: mounts, assets, then
      the scene.
- [x] A mount whose directory no longer exists is skipped with a logged
      warning rather than failing the whole load.

**Done when:** Save Session then Open Session round-trips mounts, the
active scene, and every known asset exactly, verified manually including on
the Linux path.

---

## Phase 9 — Viewport navigation & window ergonomics

**Goal:** a level bigger than the editor window is still fully reachable,
and the editor's own window size stops being confused with the target
game's window size.

- [x] Resizable editor window (opt-in `VideoSystem::Initialize` parameter,
      default off for every other consumer). The viewport is re-derived
      from the live window size every frame rather than off
      `SDL_EVENT_WINDOW_RESIZED`'s own payload, which proved to lag the
      renderer's actual output size during a live drag-resize or maximize.
- [x] Camera pan (middle-mouse-drag) and zoom-to-cursor (scroll wheel,
      clamped 0.1–10×), writing into the shared `video::Camera`; a
      "Restore View" button resets to origin/1×.
- [x] Two preview overlays: `DrawGameWindowPreview` (one letterboxed box,
      anchored at world origin, for the configured target resolution) and
      `DrawCameraOverlays` (one box + center marker per entity carrying a
      `Camera` component, mirroring `CameraSystem`'s real centering math).
      Surfaced and fixed a genuine engine bug along the way: `CameraSystem`
      centered on `Transform`'s top-left corner instead of a Sprite's
      visual middle — it now follows `SpriteGetDstRect`'s center when one
      resolves (`RenderSystem.cpp`, with new `CameraSystemTest` coverage).
- [x] View menu (Grid / Game Window) opening modal dialogs with
      Apply/Close, replacing the fields that used to sit in the Scene
      panel. Three grouped, always-on-top HUD panels (mouse position;
      Grid/Game Window readout; Restore View) anchored together at
      top-center. Grid spacing and target window size round-trip through
      `.asges`'s new `[View]` table.
- [x] Fixed a pre-existing bug this surfaced: every edge-anchored panel
      (Assets, VFS, Scene, Entities, Inspector) used `ImGuiCond_FirstUseEver`
      for position, which only applies once — none actually tracked the
      window edge across a resize despite recomputing the target position
      every frame. Switched to `ImGuiCond_Always`, matching the one
      existing precedent (`ConsolePanel`'s bottom-edge anchor).
- [x] Disabled ImGui's `imgui.ini` (`IniFilename = nullptr`) — it was
      silently overriding every `FirstUseEver` layout hint after the first
      launch.

**Done when:** a level larger than the editor window is fully reachable by
panning/zooming, resizing the window never distorts the view or drifts
anchored panels, and the target game's actual window is previewable at a
glance.

---

## Phase 10 — Asset path dropdowns & resolver correctness

**Goal:** an asset-owning field is always picked from what's actually known
to be loaded, never hand-typed, and changing or clearing that pick actually
takes effect immediately.

- [x] Sprite/Animation/AudioSource path fields are dropdowns
      (`DrawAssetPathCombo`) restricted to known asset paths plus "None",
      pre-selecting the component's existing value. Audio gained its own
      known-paths tracking, previously unexposed by the Assets panel.
- [x] The resolve-staleness fix this phase was written to address turned
      out already shipped upstream (v0.8.4's `Resolver<T>` re-resolve/clear
      tracking) — no engine change was needed for this phase.
- [x] A changed dropdown selection now propagates through the same
      `componentsChanged` → `ResolveAssets` plumbing the "x" removal button
      already used, instead of never firing or re-resolving every keystroke.

**Done when:** every asset field is picked from a dropdown, a scene's
existing paths show pre-selected, and a changed selection takes effect
immediately.

---

## Explicitly deferred — do not build until a concrete need forces it

Consistent with "no speculative abstraction, no second consumer, no
justification": these are known future wants, not phase-0 requirements.

- Undo/redo
- Multi-select / box-select
- Prefab or additive scene loading in the editor (mirrors the engine's own
  "Scene management Phase 2+" being unbuilt)
- Any generic reflection/serialization-schema-export mechanism replacing
  the hand-written `Serializer<T>` / `DrawInspector` pairing
- SDL_GPU backend for the editor (stay on `imgui_impl_sdlrenderer3` until
  the engine's own SDL_GPU migration is actually underway)

---

## Working notes for whoever (Claude) picks this up mid-roadmap

- Each phase should be raised as its own working session/PR — don't jump
  ahead to Phase 3 machinery while Phase 2's round trip is still unverified.
- Every new editable component type is one `DrawInspector` function next to
  its `Serializer<T>` specialization, not a change to a generic system.
- The editor never gains knowledge of `IGameState`/`Game<TStateId>` — if a
  task seems to require that, it's probably solving the wrong problem.
- If the raw `SDL_Renderer*` accessor added in Phase 0 wants to become a
  bigger seam (e.g. for SDL_GPU), that's a signal to revisit, not a
  reason to over-abstract now.
