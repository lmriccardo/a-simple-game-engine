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

## Phase 6 — Editor polish: component editing, native dialogs, diagnostics

Everything that came out of actually using Phase 5's editor day to day,
rather than a planned milestone — each item below is a gap that only
showed up once entities could be created/saved/reopened for real.

- [x] Add/remove components on an existing entity from the Inspector, not
      just create/delete/duplicate the entity itself — a combo of whichever
      serializable types the selection doesn't already have (default-
      constructed on click) plus a per-section "x" to remove one, both
      driven by one type-erased `Has`/`Add`/`Remove` table shared with
      `DrawSection<T>` rather than a second hardcoded type list.
- [x] No scene is auto-loaded on startup — the editor opens genuinely empty
      (same state as File > New), replacing the Phase 1/2 hardcoded test
      fixture that only ever existed on the machine that built it.
- [x] File > Save/Save As/Open all go through native OS file dialogs
      (`SDL_Show{Save,Open}FileDialog`) instead of an ImGui text-input
      popup that silently wrote under a scratch temp directory. Save
      behaves like Save As until the active scene has a real saved
      location (`nullopt` disk path). `SceneManager::LoadScene` only takes
      a virtual path, so Open (re)mounts the chosen file's parent directory
      as a virtual root each time, unmounting the previous one first.
      `editor/FileDialog.hpp/.cpp` extracts the async result hand-off
      (`FileDialogResult`/`OnFileDialogResult`) shared by Save/Save As/Open/
      the log console's own Save button below, rather than duplicating a
      mutex-guarded callback a third time.
- [x] Editor window/taskbar icon and `.exe` icon set from the project logo
      (`docsite/site/assets/img/logo.svg`, rasterized since no SVG loader
      is available at either build or runtime).
- [x] In-editor log console: `Logger` gained an `OnLog` signal (existing
      `Signal`/`Connection` pattern, not a new mechanism) that
      `editor/ConsolePanel` connects to once at startup, keeping the last
      500 `LogRecord`s in a mutex-guarded ring buffer — there's no terminal
      to read `LOG_*` output from once the editor is launched normally.
      Found and fixed a real engine bug along the way: `FormatTimestamp`
      ignored its own timestamp argument and always formatted "now",
      invisible until something (this panel) re-rendered a stored
      `LogRecord` every frame instead of printing it once.
- [x] Log panel anchored flush to both side edges and the bottom of the
      editor window — forced every frame via `SetNextWindowSizeConstraints`
      (width's min/max equal, so only height is draggable) — with a
      horizontal scrollbar for lines wider than the panel, colored
      error/warning count badges (drawn shapes next to Clear, not title-bar
      text — ImGui can't host a custom draw call there) and a right-anchored
      "Save Log" button that writes the current buffer to a `.log` file
      through the same native-dialog plumbing as Save/Open.

**Done when:** every entity's exact component makeup is editable from the
Inspector alone, opening/saving a scene never touches an invisible scratch
path, and every `LOG_*` message the editor produces is visible somewhere on
screen instead of only on a terminal that may not exist.

---

## Phase 7 — Asset awareness

- [x] `editor/AssetBrowser` lists every texture/animation-clip virtual path
      the editor currently knows about, in two sections. Went through three
      designs before landing here: first a live recursive scan of the
      current scene's own folder (broke on a real project where scenes and
      assets are siblings under a shared root, e.g.
      `assets/scenes/level.toml` referencing `assets/characters/...`,
      neither nested in the other — found nothing despite the scene's
      sprites rendering correctly); then a scan of *every* mounted root
      (correct, but showed every file under a mount whether or not it was
      actually an asset anyone cared about); settled on driving the list
      from the registry itself instead of the filesystem at all — every
      distinct, non-empty `Sprite::m_VirtualPath`/`Animation::m_ClipPath`
      currently in use, so nothing shows up that isn't actually meaningful
      to this scene.
- [x] "Load Asset..." covers the gap the registry-driven list can't: an
      empty or freshly-opened scene has no entities to derive anything
      from at all. Picks a mount first (native dialogs have no notion of a
      virtual root, so the mount supplies both the dialog's starting
      folder and the prefix for the resulting virtual path), then opens a
      native open-file dialog scoped to that mount's real directory; the
      picked file is classified the same way the old directory-scan design
      did (image extension -> texture; `.toml` containing a `[FrameTable]`
      table -> animation clip, since a scene file and a clip file are both
      plain `.toml` and only distinguishable by content). Imported paths
      go into a small persistent set alongside the registry-derived ones,
      with their own "x" to un-import — the "x" only shows for entries
      actually in that persistent set, since a purely registry-derived one
      has nothing here to remove; it'd just reappear next frame from the
      entity itself. `ImGuiSelectableFlags_AllowOverlap` was needed on the
      row's `Selectable` for that "x" to be clickable at all — a plain
      Selectable's hit box otherwise spans the full row and silently eats
      clicks meant for anything drawn on top of it further right.
- [x] A scene that finishes loading registers every asset path it
      references into that same persistent set (`RegisterSceneAssets`) --
      without this, an asset that only ever appeared because some entity
      referenced it (never actually "Load Asset..."-ed) vanished from the
      panel the instant that entity's component was removed or the entity
      deleted, even though nothing about the asset itself changed. An
      asset's presence in the panel isn't supposed to depend on whether
      anything currently happens to be using it.
- [x] Clicking an entry no longer assigns it to the selected entity —
      instead it opens `editor/AssetInspector`, a panel showing the
      asset's absolute path, mountpoint, file size, and (for textures)
      pixel dimensions, then a full-width separator and a thumbnail
      preview below it (no preview for animation clips). The preview loads
      through the same `AssetManager::GetImage` + `CreateTexture` path
      Sprite resolution itself uses, `ImGui::Image`'d via the SDL3
      renderer backend's documented convention of using a raw
      `SDL_Texture*` (`ITexture::NativeHandle()`) as the texture ID
      directly; cached and reloaded only when the inspected path changes,
      since `CreateTexture` allocates a fresh GPU texture on every call.
      Has its own title-bar close button, clearing the caller's selection
      back to "nothing inspected" rather than the panel just staying open
      forever once something's been clicked once.
- [x] Assignment instead happens at Add Component time: adding a `Sprite`
      now requires picking one of the known texture paths from a combo
      first (`editor/AssetBrowser`'s `KnownTexturePaths`) — "Add Component"
      stays disabled with a hint if nothing's loaded yet — rather than
      starting from a blank, unresolved `m_VirtualPath` the user would
      have had to fill in by hand anyway. Animation clips still use the
      older manual-text-field approach for now; only Sprite was asked for.
- [x] `editor/VfsPanel`: an always-on panel (not tied to a loaded scene)
      listing `VirtualFileSystem`'s current mounts, letting the user add
      new ones (a name plus `SDL_ShowOpenFolderDialog`), and surfacing any
      virtual root the *currently loaded* scene's Sprite/Animation/
      AudioSource paths reference but isn't mounted — a real gap Phase 7
      first shipped without: a scene authored by an actual game references
      whatever mount *that game's own code* set up (e.g. `assets` pointing
      at its own project folder), which the editor can't see or infer from
      the scene file's location alone, so opening a real-world scene (not
      one round-tripped through the editor's own "opened" convention)
      logged "mount not found" for every asset. `AssetManager`/`AssetPool`
      never cache a resolve failure (see `AssetPool::GetOrLoad`'s own doc
      comment) and `Resolver<T>`'s guard only checks success, not "already
      tried" — so nothing needed to change engine-side to make a fresh
      mount retroactively fix already-failed assets; `ResolveAssets` just
      needed calling again after the mount. That surfaced the real risk:
      `ResolveAssets` was briefly called unconditionally every frame (this
      phase's first pass), which would re-log the same failure 60 times a
      second for anything that stayed broken. Fixed by calling it only at
      specific triggers instead — scene loaded, `Inspector`'s Add/Remove
      Component (`EntityAction::ComponentsChanged`, new), an asset-browser
      pick, and a successful mount — never unconditionally per frame.
      Verified end to end with a hand-authored scene referencing an
      unmounted root: the panel correctly flagged it as Missing, mounting
      the right folder made the sprite resolve and render with no further
      errors logged.
- [x] Adding a mount under a name that's already bound to a different real
      directory now replaces it instead of sitting alongside it as a second
      candidate `Resolve()` might try first and get a stale answer (or
      none) from — `Mount()` on its own just appends. Hit this re-pointing
      the default `assets` mount (which used to start bound to the editor's
      own scratch dir) at a real project's actual assets folder: both ended
      up mounted under the same name, and since `Resolve()` tries matches
      in registration order, the AssetBrowser also needed to scan every
      mount rather than just the newest one (previous item) for this
      specific case to surface at all.
- [x] The editor no longer auto-creates any mount, ever — no `assets`
      scratch-dir mount on startup, and File > Open no longer leaves an
      `opened` mount pointing at whatever directory a scene was last
      opened from either. Both existed only because `SceneManager::
      LoadScene` takes a virtual path (unlike `SaveScene`, which takes a
      real one directly), and turned out to actively fight the VFS panel:
      a "replaced" `assets` mount from the previous item is exactly the
      kind of surprise a silent auto-mount produces once the user is
      expected to manage mounts themselves. Open now resolves its file via
      `LoadSceneFromRealPath`, which mounts a private, uniquely-named root
      just long enough for that one `LoadScene` call and unmounts it again
      immediately after — nothing user-visible is bound as a side effect,
      so every entry the VFS panel ever shows is one the user put there.
      Verified: a fresh launch's Mounted list is empty, and opening a scene
      whose assets reference an unmounted root now correctly shows it under
      Missing immediately, with no residual mount masking the gap.

**Done when:** authoring a new entity's visuals doesn't require knowing
asset filenames by memory. Verified end to end: saved a scene next to a
real `.png` and a `[FrameTable]`-bearing `.toml`, both appeared correctly
classified in the browser, clicking the texture set the Sprite's virtual
path and it rendered in the viewport with no resolve error logged.

---

## Phase 8 — Editor session persistence (`.asges`)

**Goal:** reopening the editor on a project already being worked on doesn't
mean re-adding every mount point by hand and re-opening the same scene file
— one Save Session / Open Session round trip restores exactly that working
state.

- [x] `.asges` session file format: plain TOML, going through the exact same
      `asge::toml` machinery scene files already serialize through — no
      second parser, no invented binary format for what's really two flat
      lists. A `[[Mount]]` table array (`Name`, `RealDirectory` — one entry
      per (name, dir) pair, since Phase 7's VFS panel already established
      that a root can legitimately map to more than one real directory) and
      a `[Session]` table with `ScenePath` (the currently open scene's
      absolute real path, omitted if none is open). Grew a third piece
      beyond the original scope once actually used: `[[Texture]]`/
      `[[Animation]]` table arrays (`Path`) holding every asset
      `editor/AssetBrowser`'s panel currently knows about — not just what
      the open scene's entities reference. Without these, an asset imported
      via "Load Asset..." but never assigned to any entity vanished from the
      Assets panel on the next Open Session, since `RegisterSceneAssets`
      alone can only re-derive paths from the reloaded scene's registry.
      `editor/SessionManager.hpp/.cpp` is the new module holding
      `SaveSession`/`LoadSession` plus `LoadSceneFromRealPath` (moved here
      verbatim from `main.cpp`'s anonymous namespace, since Open Session
      needed it as a second call site alongside Open Scene's existing one).
- [x] "Save Session..." / "Open Session..." in the same File menu as
      Save/Open Scene, reusing the same native-dialog + `FileDialogResult`/
      `DrainFileDialogResult` plumbing every other dialog in the editor
      already goes through, filtered to `*.asges`. Both dialogs (and, once
      the same latent bug was spotted there too, Save/Open Scene's own
      pre-existing dialogs) gained a guard against a native backend handing
      back an empty or directory-only path instead of failing outright —
      without it, saving would silently write a bare `.asges`/`.toml` file
      instead of erroring.
- [x] Save Session writes every current `VirtualFileSystem::ListMounts()`
      entry, every path `AssetBrowser::KnownTexturePaths`/
      `KnownAnimationPaths` currently return, plus `currentScenePath` (if
      set), to the chosen file.
- [x] Open Session unmounts everything currently mounted, mounts every
      `[[Mount]]` entry from the file, restores every `[[Texture]]`/
      `[[Animation]]` path into the Assets panel via the new
      `AssetBrowser::ImportAssets` (a path already there from the
      about-to-load scene's own usage is just a no-op `std::set` insert, not
      a duplicate entry), then — if `ScenePath` is present — loads that
      scene the same way Open Scene does (`LoadSceneFromRealPath`,
      `ResolveAssets`, `RegisterSceneAssets`). Replaces the current working
      state outright rather than merging with it, matching what Open Scene
      already does to `currentScenePath`.
- [x] A `[[Mount]]` entry whose `RealDirectory` no longer exists (moved or
      deleted on disk since the session was saved) is skipped with a
      logged warning instead of mounting a dead path or failing the whole
      load. The file is parsed in full before any live state is touched
      (mounts unmounted, scene unloaded), so a bad/missing `.asges` itself
      can't half-clobber the editor either.

**Done when:** Save Session then Open Session round-tripped mounts, the
active scene, and every known asset (including ones unused by any entity)
exactly; a moved/missing mount directory logged a warning instead of
failing silently or aborting the rest of the load; and an empty/canceled
save dialog no longer wrote a nameless `.asges`/`.toml` file. Verified
manually end to end, including on the WSL/Linux path via the new
`linux-editor` CMake preset.

---

## Phase 9 — Viewport navigation & window ergonomics

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

## Phase 10 — Asset path dropdowns & resolver correctness

**Goal:** an asset-owning field (Sprite/Animation/AudioSource today, whatever
else grows one later — a Font component included) is always picked from
what's actually known to be loaded, never hand-typed, and changing or
clearing that pick actually changes what renders/plays instead of leaving a
stale previously-resolved handle in place.

- [x] Sprite/Animation/AudioSource's virtual-path fields (`m_VirtualPath`,
      `m_ClipPath`, `m_VirtualClipPath`) are dropdown selections in the
      Inspector (`DrawAssetPathCombo`) instead of raw `DrawTextField` text
      inputs — restricted to whatever `AssetBrowser`'s `KnownTexturePaths`/
      `KnownAnimationPaths`/`KnownAudioPaths` actually knows about, with an
      explicit "None" entry for "no asset yet." Audio wasn't exposed by
      `AssetBrowser` at all before this — it gained its own "Audio Clips"
      section, `KnownAudioPaths`, and a matching `[[Audio]]` table in the
      `.asges` session format, using `AssetKind::AudioClip` out of the
      already-existing `CollectAssetRefs` (the engine was already reporting
      it; nothing in the editor was listening). A freshly loaded scene's
      already-set path appears pre-selected, not reset to "None" — the
      combo's current index is whichever `KnownXPaths()` entry matches the
      component's existing field. `DrawAddComponentControl`'s own
      `##SpriteTexture` combo at Sprite-add time was left as-is (no "None"
      entry there by design — a Sprite is always given a real texture up
      front) rather than unified with the Inspector's combo, since the two
      have different mandatory-vs-optional semantics.
- [x] The underlying resolve-staleness bug this surfaced turned out to
      already be fixed — merging main's v0.8.4 brought `Resolver<Sprite>`/
      `Resolver<Animation>`/`Resolver<AudioSource>` forward with exactly
      this behavior already built in: each now tracks
      `m_ResolvedVirtualPath`/`m_ResolvedClipPath`/
      `m_ResolvedVirtualClipPath` alongside `m_Texture`/`m_Clip` and
      re-resolves, or clears back to `nullptr`/`nullopt`, whenever the
      component's current path differs from what it last resolved. No
      `src/ASGE/` change was needed for this phase.
- [x] `editor/Inspector.cpp`'s `DrawInspector(Sprite&)`/
      `DrawInspector(Animation&)`/`DrawInspector(AudioSource&)` now return
      `bool` (true only when the path dropdown's selection changed, not for
      an unrelated edit like Animation's Frame Duration), and `DrawSection<T>`
      forwards extra args (the known-paths list) to `DrawInspector` and
      detects a `bool` return via `decltype`/`if constexpr` — so a changed
      selection propagates through the same `componentsChanged` plumbing
      the "x" removal button already used, triggering `AssetManager::
      ResolveAssets` on selection-changed rather than never (the old
      `DrawTextField`-discarding behavior) or on every keystroke.

**Done when:** every Sprite/Animation/AudioSource in the Inspector is
assigned its asset by picking from a dropdown of known assets (or "None")
rather than typing one by hand; a scene's existing paths show up
pre-selected on load; and changing a selection (including back to "None")
actually changes what's rendered/played immediately, rather than leaving a
stale previously-resolved handle in place.

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