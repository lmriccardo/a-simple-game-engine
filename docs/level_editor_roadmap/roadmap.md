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

- [ ] For each additional component type you want editable (`Collider`,
      `Animation`, `Sprite`/layer fields), write one
      `void DrawInspector(ComponentType&)` free function — mirrors your
      existing convention of one `Serializer<T>` specialization per type,
      not a generic reflection system. No macro-based reflection unless a
      second unrelated consumer justifies it later.
- [ ] Entity list panel: fixed, explicit list of known component types
      checked via `Registry::GetPool<T>()->Has(entity)` per type — same
      "no RTTI, function-local static counter IDs" convention already used
      internally by `Registry`.
- [ ] Selecting an entity in the list drives the same inspector panel as
      viewport picking (Phase 2) — one selection state, two input paths.

**Done when:** you can select any entity from the list or viewport and
edit every currently-serializable component type on it, with each edit
round-tripping through Phase 2's proven save path.

---

## Phase 4 — Visual collider overlays & basic gizmos

- [ ] World-space coordinate grid, drawn as an `ImDrawList` overlay in the
      viewport via the same `Camera`/`Viewport` math Phase 2's picking
      already reuses (`WorldToScreen`, so grid lines pan/zoom with the
      camera instead of staying screen-fixed) — right now position is only
      knowable by opening the inspector and reading numbers off a selected
      entity, with nothing in the viewport itself to place it against.
- [ ] Draw `Collider` bounds (AABB/circle, per your existing collision
      types) as translucent `ImDrawList` overlays in the viewport, colored
      by layer/mask bitfield — this is the actual visual value-add over a
      coordinates-only editor.
- [ ] Simple translate gizmo (drag handles, not just raw position drag) if
      free-drag from Phase 2 feels imprecise in practice. Skip this if
      free-drag is already good enough — don't build a gizmo system
      speculatively.

**Done when:** you can visually confirm collider placement/overlap, and
place/read off an entity's world position via the grid, without switching
to the running game.

---

## Phase 5 — Entity lifecycle: create, delete, duplicate

- [ ] "Create entity" goes through the same creation path gameplay code
      uses (`Registry::Create()` + attach components) — no separate
      editor-only construction API.
- [ ] Delete selected entity (`Registry::Destroy` or equivalent).
- [ ] Duplicate selected entity (copy each present component's data into a
      newly created entity) — straightforward once Phase 3's per-type
      component enumeration exists.
- [ ] "File > New": resets to a genuinely empty `Registry` and clears the
      current scene path, instead of hand-deleting every entity out of
      whatever the editor happened to load at startup. Without this, "an
      empty scene" in the Done-when below is only reachable by editing the
      Phase 1/2 hardcoded test scene down to nothing.
- [ ] "File > Save As": prompts for a destination path instead of Phase 2's
      Save always overwriting wherever the current scene was loaded from —
      otherwise a new-from-scratch level has nowhere of its own to be saved.

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