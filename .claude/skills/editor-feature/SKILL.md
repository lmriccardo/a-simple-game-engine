---
name: editor-feature
description: Implement a feature, fix, or roadmap phase in ASGE's level editor (the editor/ directory, asge-editor). Use whenever asked to add, change, or fix something in the level editor, or to work on a docs/level_editor_roadmap phase.
---

Standing conventions for `editor/` (the Dear ImGui-based `asge-editor` tool),
built up over the course of implementing it phase by phase. These apply in
addition to — not instead of — the repo's own `CLAUDE.md`.

## 0. Scope discipline

- Implement one phase or feature at a time. **Never silently move on to the
  next phase** once one finishes — stop and tell the user it's done so they
  can review/test it, per the standing instruction from when this editor's
  build started.
- The editor never gains a dependency on `Game<TStateId>`/`IGameState` —
  `Application` is templated on `TGame` and unusable here; the editor drives
  its own raw SDL event loop directly against `VideoSystem`. If a task seems
  to need `IGameState`, it's solving the wrong problem.
- No speculative abstraction: prove the narrow case before generalizing
  (`docs/level_editor_roadmap/roadmap.md`'s own stated philosophy). A new
  editable component type is one `DrawInspector` free function next to its
  `Serializer<T>` specialization in `editor/Inspector.cpp` — not a change to
  a generic reflection system.

## 1. Implement

Read the actual affected files first, same as anywhere else in this repo.
Known ImGui gotchas that have come up repeatedly here:

- Two components sharing a field label (e.g. two "Layer" fields) collide
  onto the same widget ID unless each section is wrapped in its own
  `ImGui::PushID`/`PopID`.
- `ImGui::OpenPopup` and `BeginPopupModal` must be called at the same
  ID-stack depth — calling `OpenPopup` from inside a menu but
  `BeginPopupModal` outside it hashes to a different ID, and the popup
  silently never opens. Defer the actual `OpenPopup` call to just outside
  the menu via a bool flag set inside it.
- A native `SDL_Show{Open,Save}FileDialog`/`SDL_ShowOpenFolderDialog` call is
  async — its callback may run on a different thread. Hand results off
  through `editor/FileDialog.hpp`'s `FileDialogResult`/`DrainFileDialogResult`
  rather than inventing another mutex-guarded round trip.

`components::SerializableComponents` (`src/ASGE/Game/Components.hpp`) is the
one fixed list of every serializable component type — the scene
(de)serializer and various editor folds (`DuplicateEntity`, the asset
browser's asset-owning-component scan, etc.) all walk this same tuple.
Adding a new serializable component means adding it there, not inventing a
second list.

## 2. Build, and do a compile check only

```
cmake --preset windows-editor
cmake --build --preset windows-editor-debug --target asge-editor
```

If the change touched `src/ASGE/` itself (not just `editor/`), also run the
engine's own suite: `ctest --preset windows-editor-debug`. Skip this for
editor-only changes — it's a needless few-minutes cost for code the suite
doesn't cover.

A locked `asge-editor.exe` (`LNK1168` at link time) means an earlier launch
was never stopped — stop it first (`.claude/skills/run-asge/driver.ps1
stop`, or `Stop-Process -Force` by name) before rebuilding.

## 3. Launch it for the user, then stop

```
powershell -File .claude/skills/run-asge/driver.ps1 launch -Exe bin/Debug/asge-editor.exe
```

**Then stop.** Tell the user it's built and running, and briefly what to
try. Do **not** go on to drive the UI yourself with synthetic mouse/keyboard
input (`SetCursorPos`/`mouse_event`/`SendKeys`) and screenshots to verify the
feature end to end — that automated verification loop is explicitly not
wanted here. Wait for the user to close the editor and report back before
taking any further action.

The one exception: a quick sanity check that isn't full feature verification
(e.g. "did it even open without crashing") is fine to ask about, but ask
first rather than defaulting back into a screenshot-and-click loop.

## 4. Roadmap bookkeeping

`docs/level_editor_roadmap/roadmap.md` tracks phases as checklists. Once the
user confirms a feature or phase is good:

- Check off the relevant item(s), each with a short implementation note —
  what was built, plus any non-obvious discovery or deviation found along
  the way — matching the style of every other checked item in that file.
- A fix or follow-up requested *after* a phase was already checked off gets
  appended as a new checked item under that same phase (or filed as a new
  phase, if the user says so). Don't silently rewrite an already-checked
  item's own text unless correcting something it actually got wrong.

## 5. Git workflow

Only `git commit`/`git push` when the user explicitly says so ("commit and
push" or equivalent) — never proactively, even right after they confirm a
feature works. When they do:

- Verify the current branch isn't `main` first (protected, but a push can
  bypass that silently).
- Split the diff into logical commits the way the rest of this repo's
  history does: engine changes (`src/ASGE/`) separate from editor changes
  (`editor/`) separate from docs (`docs/level_editor_roadmap/roadmap.md`),
  each following `CLAUDE.md`'s gitmoji + Conventional Commits format with a
  narrative body explaining *why*, not just *what*.
