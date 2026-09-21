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

- [x] Vendor Dear ImGui (core + `backends/imgui_impl_sdl3.cpp` +
      `backends/imgui_impl_sdlrenderer3.cpp`) alongside existing vendored
      libs (`stb_image`, `stb_truetype`, TOML parser).
- [x] New CMake target `asge-editor`, linking against the `Core` library
      target (whatever it's currently called) — not `Game`.
- [x] Add `IRenderer::GetNativeHandle()` (or equivalent) to expose the raw
      `SDL_Renderer*` — the one sanctioned leak through the `IRenderer`
      abstraction, needed because ImGui's SDL3 renderer backend binds to it
      directly.
- [x] Minimal `main()`: SDL3 init → create window/renderer via existing
      engine init path → `ImGui_ImplSDL3_Init` / `ImGui_ImplSDLRenderer3_Init`
      → loop that pumps events (`ImGui_ImplSDL3_ProcessEvent` first), starts
      an ImGui frame, draws a single `ImGui::ShowDemoWindow()`, renders, and
      presents.

**Done when:** empty engine window with ImGui's demo window rendering on
top, no crashes on close.

---

## Phase 1 — Load a real scene into the editor

**Goal:** prove the editor operates on genuine engine state, not a mock.

- [x] Remove the demo window. On startup, call the existing
      `SceneManager::Load(path)` (same path the game uses) into a real
      `Registry`, from a hardcoded test `.toml` for now.
- [x] Run the existing render passes (whatever your game loop already calls
      — texture draw, sprite/animation draw) each frame, so the loaded
      scene actually renders in the window.
- [x] ImGui pass on top is just a single "Scene loaded: N entities" text
      window, reading entity count via `Registry`.

**Done when:** a real authored `.toml` scene renders correctly inside the
editor executable, using the unmodified render pipeline.

---

## Phase 2 — Prove the round trip on one component: `Transform`

**Goal:** the single most important milestone — confirm select → edit →
save → reload works end to end, before building any generalized machinery.
This is the "concrete two-pool View before variadic" moment for the editor.

- [x] Screen→world inverse transform using the camera/projection math
      already live in the renderer (reuse, don't re-derive).
- [x] Mouse click hit-tests against each entity's `Transform` position
      (+ sprite bounds if available) to pick a single entity. No multi-select
      yet.
- [x] ImGui inspector window: if an entity is selected and has a
      `Transform`, show `ImGui::DragFloat2` bound to its position fields.
      Direct mutation of the live component in the `Registry` — no
      intermediate copy.
- [ ] Optional at this stage: dragging the entity directly in the viewport
      (convert mouse delta → world delta → write into `Transform`) instead
      of only the ImGui drag fields. Skipped — DragFloat2 already covers
      editing; add if the fields prove too fiddly in practice.
- [x] "Save" menu item calls the existing save path (`Serializer<Transform>`
      → `TOMLTableView` → file write) — exactly the code the game's save
      path already uses, unmodified.
- [x] Manually verify: edit position in editor → save → load the file with
      the actual game (not the editor) → entity is in the new position.

**Done when:** that manual verification passes. This is the phase that
proves there is no schema duplication — skip it and everything after is
built on an unverified assumption.

---

## Phase 3 — Generalize the inspector to more component types

**Only start this once Phase 2's round trip is proven and boring.**

- [x] For each additional component type you want editable (`Collider`,
      `Animation`, `Sprite`/layer fields), write one
      `void DrawInspector(ComponentType&)` free function — mirrors your
      existing convention of one `Serializer<T>` specialization per type,
      not a generic reflection system. No macro-based reflection unless a
      second unrelated consumer justifies it later. Covers every
      currently-serializable type (`editor/Inspector.cpp`): `Transform`,
      `Velocity`, `Rigidbody`, `Sprite`, `Collider`, `Camera`,
      `AudioSource`, `Animation`, `PathFollow`. `Registry::GetPool<T>()`
      doesn't actually exist — used `Registry::GetComponent<T>` per type
      instead, which doubles as the "does it have one" check.
- [x] Entity list panel: fixed, explicit list of known component types
      checked via `Registry::GetPool<T>()->Has(entity)` per type — same
      "no RTTI, function-local static counter IDs" convention already used
      internally by `Registry`.
- [x] Selecting an entity in the list drives the same inspector panel as
      viewport picking (Phase 2) — one selection state, two input paths.

**Done when:** you can select any entity from the list or viewport and
edit every currently-serializable component type on it, with each edit
round-tripping through Phase 2's proven save path.

---

## Phase 4 — Visual collider overlays & basic gizmos

- [x] World-space coordinate grid, drawn as an `ImDrawList` overlay in the
      viewport via the same `Camera`/`Viewport` math Phase 2's picking
      already reuses (`WorldToScreen`, so grid lines pan/zoom with the
      camera instead of staying screen-fixed) — right now position is only
      knowable by opening the inspector and reading numbers off a selected
      entity, with nothing in the viewport itself to place it against.
- [x] Draw `Collider` bounds (AABB/circle, per your existing collision
      types) as translucent `ImDrawList` overlays in the viewport, colored
      by layer/mask bitfield — this is the actual visual value-add over a
      coordinates-only editor.
- [x] Viewport free-drag: click-and-hold a selected entity directly in the
      viewport (reusing Phase 2's picking) and move the mouse to reposition
      it, converting screen-space mouse delta to world-space delta via the
      same `Camera`/`Viewport` math the grid/picking already use, and
      writing straight into `Transform` — no intermediate copy, same as the
      inspector's `DragFloat2`. Unconstrained (no axis lock, no snapping);
      that's what a gizmo would add on top, not this.
- [x] Translate gizmo on the selected entity: X/Y arrow handles drawn at its
      `Transform` position via the same `ImDrawList` overlay approach as the
      grid/collider bounds above. Dragging the X (or Y) arrow moves the
      entity along just that axis; a center handle (or free-drag itself)
      still allows unconstrained movement. Wanted outright, not
      conditional on free-drag feeling imprecise — keep it in scope rather
      than skipping it.

**Done when:** you can visually confirm collider placement/overlap, and
place/read off an entity's world position via the grid, without switching
to the running game.

---

## Phase 5 — Entity lifecycle: create, delete, duplicate

- [x] "Create entity" goes through the same creation path gameplay code
      uses (`Registry::Create()` + attach components) — no separate
      editor-only construction API. Attaches a bare `Transform` (the
      minimum to be visible/pickable) plus a `SceneId` tag — required so
      `SaveScene()` (which filters by `ActiveEntities()`, SceneId-based)
      doesn't silently drop the new entity; not a new construction API
      itself, just `Registry::AddComponent` a second time.
- [x] Delete selected entity (`Registry::Destroy` or equivalent).
- [x] Duplicate selected entity (copy each present component's data into a
      newly created entity) — straightforward once Phase 3's per-type
      component enumeration exists. Folds over the same public
      `components::SerializableComponents` tuple `SceneManager` itself
      already uses for this (its own copy is private), plus the same
      `SceneId` tagging `Create` needs.
- [x] "File > New": resets to a genuinely empty `Registry` and clears the
      current scene path, instead of hand-deleting every entity out of
      whatever the editor happened to load at startup. Without this, "an
      empty scene" in the Done-when below is only reachable by editing the
      Phase 1/2 hardcoded test scene down to nothing. Needed a small
      addition to `SceneManager` itself (`RenameActiveScene`) — nothing in
      its public API could establish a fresh active scene identity with no
      entities without either loading real content from disk or already
      being resident.
- [x] "File > Save As": prompts for a destination path instead of Phase 2's
      Save always overwriting wherever the current scene was loaded from —
      otherwise a new-from-scratch level has nowhere of its own to be saved.
      Also retags every active entity's `SceneId` to the new path via the
      same `RenameActiveScene`, so `Save`/`ActiveEntities()` keep seeing
      them post-rename instead of silently going empty.

**Done when:** a level can be authored from an empty scene — via File > New,
not by deleting everything out of an existing one — and saved to a new file
of its own via File > Save As.

---

## Phase 6 — Asset awareness

- [ ] Read-only panel listing textures / `FrameTable` sidecars known to
      `AssetManager`, so you can see what's available without leaving the
      editor.
- [ ] Assigning a sprite/animation to an entity picks from this list
      instead of typing a path string into a `DragFloat`-style field.

**Done when:** authoring a new entity's visuals doesn't require knowing
asset filenames by memory.

---

## Phase 7 — Viewport navigation & window ergonomics

**Goal:** a level bigger than the editor window is still fully reachable,
and the editor's own window size stops being confused with the target
game's window size — two independent things today's editor conflates by
having neither.

- [ ] Resizable editor window: opt-in via a new `VideoSystem::Initialize`
      parameter (default `false`, so every other consumer — examples,
      games — keeps today's fixed-size behavior unchanged) rather than
      flipping `SDL_CreateWindow`'s flags engine-wide; `asge-editor` is the
      one caller that requests it. On `SDL_EVENT_WINDOW_RESIZED`, update
      `IRenderer::SetViewport(...)` to match the new size — without this,
      a resizable window would just stretch/crop the existing surface
      instead of revealing more of the world.
- [ ] Camera pan/zoom: mouse-driven viewport navigation (e.g.
      middle-mouse-drag to pan, scroll wheel to zoom) writing into the same
      `asge::video::Camera` the renderer already exposes via
      `SetCamera`/`GetCamera`. Independent of window size entirely — a
      level can always be bigger than any window, resizable or not, so
      this is the actual fix for "I can't reach the rest of my level," not
      the resize support above.
- [ ] Target-game resolution preview: a rectangle overlay (same
      `ImDrawList` approach as Phase 4's grid/collider overlays)
      representing the actual game's configured window size/aspect ratio,
      letterboxed to fit within whatever the editor's viewport currently
      is. Previews what the shipped game will actually show, without ever
      constraining the editor's own window to that size — the game's
      resolution is scene/gameplay data, not an editor-window concern.

**Done when:** a level larger than the editor window can be fully authored
by panning/zooming to reach every part of it, resizing the editor window
never distorts the view, and you can see at a glance what the target
game's actual window will show.

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