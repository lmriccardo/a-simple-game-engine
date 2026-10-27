# Docsite Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the ASGE docsite with a modern layout (same palette), a shared template layer, Overview / Engine / Editor / Releases sections, Ctrl+K search, and re-themed API pages.

**Architecture:** One page registry (`SITE_MAP`) in `docsite/_shared.py` drives the top nav, per-section sidebars, search index and the link checker. `generate_site.py` becomes a thin orchestrator over per-section content modules; `generate_api.py` keeps parsing Doxygen XML but renders through the same `render_page`. Output stays static HTML, committed under `docsite/site/` except the CI-built `engine/api/`.

**Tech Stack:** Python 3 stdlib only, vanilla JS, existing CSS tokens, highlight.js/marked/DOMPurify from cdnjs (already used), Doxygen XML (CI).

**Spec:** `docs/superpowers/specs/2026-10-27-docsite-refactor-design.md`

## Global Constraints

- Palette tokens in `docsite/site/assets/css/style.css` (`--accent #6a4cf0`, `--accent-2 #0f9c8c`, light + dark sets) are kept unchanged.
- Python stdlib + vanilla JS only; no static-site generator, no new dependency.
- No version switcher; no redirects from old URLs.
- Do not touch Doxygen input (`INPUT = ../src`) or `src/ASGE`.
- Static files work under a GitHub Pages project sub-path: all internal links are relative (`rel(depth, path)`).
- Responsive to phone width (no horizontal page scroll); search and theme toggle keyboard-reachable; contrast OK in both themes.
- Branch `refactor/docsite` (already checked out). Commit style: `<type>(docsite): <emoji> <Imperative summary>` (gitmoji + Conventional Commits per CLAUDE.md; `feat` → `:sparkles:`, `refactor` → `:recycle:`, `docs` → `:memo:`, `ci` → `:construction_worker:`, `test` → `:white_check_mark:`), body optional, ending with the `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` trailer. Verify `git branch --show-current` is `refactor/docsite` before every commit.
- Generated URLs: `/index.html`, `/engine/getting-started.html`, `/engine/theory/<slug>.html`, `/engine/examples/index.html` + `/engine/examples/<slug>.html`, `/engine/api/…`, `/editor/index.html` + `/editor/<task>.html`, `/releases.html`.
- Doxygen is **not installed locally**; API output can only be checked against XML from CI or a hand-made fixture (Task 6).

## Review Focus

- Releases API fetch fails / rate-limited (403) / returns empty: page must show a link to GitHub releases, not a blank list.
- Page with no `h2`/`h3`: on-page TOC is omitted cleanly, no empty box.
- Search query with no results, or with regex characters (`(`, `[`, `\`): "No results" shown, no JS exception.
- `engine/api/` absent (local build, no Doxygen): committed pages still link-check, with API links explicitly allowed; search works without the API index (404 ignored).
- Duplicate heading text on one page: heading ids stay unique so TOC anchors don't collide.

---

## File Structure

- `docsite/_shared.py` — modify: `Page` dataclass, `SITE_MAP`, `render_page`, `render_sidebar`, `add_heading_ids`, `build_toc`, nav. Single source of layout.
- `docsite/generate_site.py` — modify: slim orchestrator (`main()` calls each content module, writes search index).
- `docsite/content/` — create package: `overview.py`, `engine.py` (getting started + examples, `EXAMPLES` data moves here), `theory.py`, `editor.py`, `releases.py`; each exposes `build() -> dict[str, str]` mapping site-relative path → HTML.
- `docsite/generate_api.py` — modify: output to `site/engine/api/`, render through new `render_page`, emit `search-index.json` there.
- `docsite/check_site.py` — create: link/structure checker with an `--selftest`.
- `docsite/site/assets/css/style.css`, `docsite/site/assets/js/main.js` — modify; `search.js` — create (replaces `api-search.js`); `releases.js` — rewrite; delete `editor-releases.js`, `api-search.js`.
- `.github/workflows/docs.yml`, `.gitignore` — modify paths for `engine/api`.

---

### Task 1: Site checker

**Files:**
- Create: `docsite/check_site.py`

**Interfaces:**
- Produces: `check(site: Path, allow_missing: tuple[str, ...] = ()) -> list[str]` returns problem strings (empty = OK). CLI: `python docsite/check_site.py [--allow-missing engine/api/] [--selftest]`; exit 1 on problems.
- Checks per HTML page: every relative `href`/`src` resolves to an existing file (fragment-only and `http(s):`/`mailto:` skipped; prefixes in `allow_missing` skipped); exactly one `<h1>`; all `id` attributes unique within the page; each `#fragment` on a local link exists as an id in the target page.

- [ ] **Step 1: Write `selftest()`** building a temp site with four tiny pages and asserting `check()` reports: a broken link, a missing `<h1>`, a duplicate id, a dangling `#fragment`; and returns `[]` for a good page and for a link under an allowed-missing prefix.
- [ ] **Step 2: Run `python docsite/check_site.py --selftest`** — Expected: FAIL (`check` not defined).
- [ ] **Step 3: Implement `check()` and CLI** using `html.parser.HTMLParser`.
- [ ] **Step 4: Run selftest** — Expected: `selftest ok`.
- [ ] **Step 5: Commit** — `test(docsite): :white_check_mark: Add site link and structure checker`

---

### Task 2: Layout core and page registry

**Files:**
- Modify: `docsite/_shared.py`
- Modify: `docsite/check_site.py` (add a selftest case for the new helpers is not needed; see Step 1)

**Interfaces:**
- Produces:
  - `@dataclass(frozen=True) class Page: path: str; title: str; section: str; group: str; summary: str` — `section ∈ {"overview","engine","editor","releases","api"}`, `group` is the sidebar heading (e.g. `"Theory"`, `"Examples"`), `path` is site-relative.
  - `SITE_MAP: list[Page]` — filled by content modules via `register(page: Page) -> Page` (so nav/sidebar/search are derived, never hand-listed).
  - `render_page(*, page: Page, body: str, sidebar: str | None = None, extra_head: str = "", extra_scripts: str = "") -> str` — depth derived from `page.path`; header with brand, section links (Overview, Engine, Editor, Releases, GitHub), theme toggle, search button (`data-search-open`); left sidebar (`render_sidebar(page)` unless `sidebar` given); `<main>` with article + right TOC.
  - `render_sidebar(page: Page) -> str` — pages of `page.section` grouped by `group`, active page marked `aria-current="page"`.
  - `add_heading_ids(body: str) -> str` — gives every `h2`/`h3` a slug `id` (suffix `-2`, `-3` on collisions).
  - `build_toc(body: str) -> str` — `<nav class="toc">` from `h2`/`h3`; returns `""` if none (Review Focus: no-heading pages).
- Keeps: `rel`, `write`, `code_card`, `ROOT`, `SITE`, `GITHUB_URL`, `REPO`. Remove `NAV_ITEMS`, `EDITOR_PAGES`, `refresh_editor_nav`, `render_editor_menu` (callers are rewritten in later tasks).

- [ ] **Step 1: Write tests** in a new `docsite/tests_shared.py` (plain `assert` functions + `__main__` runner): `add_heading_ids` makes two identical `<h2>Setup</h2>` ids `setup` and `setup-2`; `build_toc("<p>x</p>") == ""`; `render_sidebar` marks only the current page `aria-current="page"` and lists only same-section pages; `render_page` for `Page("engine/theory/ecs.html", …)` contains `href="../../assets/css/style.css"`.
- [ ] **Step 2: Run `python docsite/tests_shared.py`** — Expected: FAIL (names undefined).
- [ ] **Step 3: Implement the interfaces above** in `_shared.py`.
- [ ] **Step 4: Run tests** — Expected: all pass.
- [ ] **Step 5: Commit** — `refactor(docsite): :recycle: Introduce page registry and shared layout renderer` (the generators are temporarily broken until Task 4/6; note this in the body).

---

### Task 3: Visual redesign (CSS + main.js)

**Files:**
- Modify: `docsite/site/assets/css/style.css`, `docsite/site/assets/js/main.js`

**Interfaces:**
- Consumes: markup from `render_page` (classes: `site-header`, `nav-links`, `layout`, `sidebar`, `article`, `toc`, `search-modal`).
- Produces: theme toggle persisted in `localStorage` (try/catch), mobile sidebar drawer (`[data-nav-toggle]`), TOC scroll-spy (adds `.active` to the visible heading's link), existing copy-button and code highlighting retained. Search UI lives in Task 8; here only add the `.search-modal` styles and the `[data-search-open]` button styling.

- [ ] **Step 1: Restyle** keeping every colour token: 3-column layout (sidebar 260px / article max 760px / TOC 220px), sticky header with blur, rounded cards, refined typography, hero treatment, 16px side gutters; sidebar becomes a drawer below 960px and TOC hides below 1200px.
- [ ] **Step 2: Extend `main.js`** with TOC scroll-spy via `IntersectionObserver` and the drawer toggle.
- [ ] **Step 3: Verify** with `python -m http.server -d docsite/site 8000` once Task 4 has produced pages; check light/dark at 1440px and 390px widths. (Reviewed by the owner in a browser; do not drive the UI with synthetic input.)
- [ ] **Step 4: Commit** — `feat(docsite): :sparkles: Redesign the site theme with sidebar and table of contents`

---

### Task 4: Overview, Getting Started and Examples

**Files:**
- Create: `docsite/content/__init__.py`, `docsite/content/overview.py`, `docsite/content/engine.py`
- Modify: `docsite/generate_site.py` (reduce to orchestrator; move `EXAMPLES`, `FEATURES`, `ROADMAP`, `build_install`, example builders into the new modules)
- Delete (regenerated elsewhere): `docsite/site/index.html`, `install.html`, `examples/`

**Interfaces:**
- Produces: `overview.build() -> dict[str, str]` (`index.html`: hero with two entry cards Engine/Editor, feature grid, roadmap, latest-release strip filled by `releases.js`); `engine.build()` (`engine/getting-started.html` from the old install page, `engine/examples/index.html` grid + one `engine/examples/<slug>.html` per entry in `EXAMPLES`, 20 pages). Example screenshots and `assets/console-output/*` stay where they are; asset links use `rel()` with the new depth (2).
- `generate_site.main()` iterates modules, `write()`s each result, and calls `write_search_index()` (defined in Task 8; stub as no-op until then).

- [ ] **Step 1: Run `python docsite/check_site.py`** after generation — Expected: FAIL before the move (old pages deleted), PASS after.
- [ ] **Step 2: Move and re-render** the content with the new `Page` registry; keep all example text/data unchanged.
- [ ] **Step 3: Run `python docsite/generate_site.py && python docsite/check_site.py --allow-missing engine/api/ --allow-missing editor/ --allow-missing engine/theory/ --allow-missing releases.html`** — Expected: no problems for the pages built so far (the allow-list shrinks as later tasks land).
- [ ] **Step 4: Commit** — `refactor(docsite): :recycle: Split page generation into content modules and move install/examples under Engine`

---

### Task 5: Theory section

**Files:**
- Create: `docsite/content/theory.py`

**Interfaces:**
- Produces: `theory.build()`; pages `engine/theory/index.html` plus:
  - Full pages (concept → how ASGE implements it → links to API pages `../api/…` → "See also"): `ecs.html` (source: `src/ASGE/Core/ECS`), `game-loop.html` (`src/ASGE/Application`, `Core/Time`, `Core/Profiling`), `events-signals.html` (`src/ASGE/Events`, `Core/Patterns/Signal.hpp`), `memory-threading.html` (`src/ASGE/Core/Memory`, `Core/Concurrent`).
  - Stubs (title, one-paragraph summary, links to API/examples, an "in progress" badge) **only for subsystems whose code exists in `src/ASGE` today**; candidates to verify against the source: Input, Rendering/Video, Audio, Filesystem/VFS, Assets, Scene, UI, Math. Drop any candidate that is only a placeholder.
- Full pages must cite real type names (e.g. `Registry`, `ThreadPool`, `LinearAllocator`, `PoolAllocator`) checked against the headers, and include at least one short code snippet each taken from a header or an example.

- [ ] **Step 1: Read the listed source directories** and `docs/roadmap/` to draft each page; do not describe unimplemented features.
- [ ] **Step 2: Implement `theory.build()`**, registering the pages with `group="Theory"`.
- [ ] **Step 3: Run `python docsite/check_site.py`** — Expected: no problems beyond allowed `engine/api/` links; every `h1` unique.
- [ ] **Step 4: Commit** — `docs(docsite): :memo: Add game engine theory section`

---

### Task 6: API pages through the new layout

**Files:**
- Modify: `docsite/generate_api.py`, `.github/workflows/docs.yml`, `.gitignore`, `docsite/site/assets/js/main.js` if API-specific hooks exist

**Interfaces:**
- Consumes: `render_page`, `Page`, `rel` from `_shared`.
- Produces: pages under `site/engine/api/` (`API_DEPTH = 2`); each built with `Page(path="engine/api/<file>", section="api", …)` and `sidebar=build_sidebar(compounds)` (the existing namespace tree) passed to `render_page`; `engine/api/search-index.json` written from `build_search_index(compounds)` (same entry shape as Task 8's index: `{"title","url","section","text"}`).
- Workflow: `.gitignore` `docsite/site/api/` → `docsite/site/engine/api/`; `docs.yml` upload path → `docsite/site/engine/api`, assemble step `path: _site/engine/api`.

- [ ] **Step 1: Create a tiny fixture** `docsite/tests/xml_fixture/` (one class compound + `index.xml`, hand-written) and a test `docsite/tests_shared.py::test_api_fixture` that sets `generate_api.XML_DIR` to it and asserts the output page contains the class name and the namespace sidebar. (No local Doxygen.)
- [ ] **Step 2: Run it** — Expected: FAIL (paths/signature not updated).
- [ ] **Step 3: Update generator, `.gitignore` and workflow.** Keep all XML-rendering logic untouched.
- [ ] **Step 4: Run tests; run `python docsite/check_site.py` against the fixture build** — Expected: pass. Real-XML verification happens in CI on the PR; say so in the commit body.
- [ ] **Step 5: Commit** — `refactor(docsite): :recycle: Render API pages through the shared layout under engine/api`; then `ci(workflows): :construction_worker: Publish the API reference under engine/api` for the workflow change.

---

### Task 7: Editor task guide

**Files:**
- Create: `docsite/content/editor.py`
- Delete: old hand-written `docsite/site/editor/*.html` (replaced by generated pages)

**Interfaces:**
- Produces: `editor.build()`; `editor/index.html` (overview with hero screenshot and task cards) plus ~10 task pages, text rewritten from the 7 old pages and `docs/level_editor_roadmap/`:
  `create-a-project`, `work-with-scenes`, `navigate-the-viewport`, `arrange-the-ui`, `add-entities-and-components`, `import-and-inspect-assets`, `add-a-collider`, `animate-a-sprite`, `follow-a-path`, `configure-the-game-window`.
- Screenshots reused from `assets/img/editor/` (e.g. `collider-*.png`, `create-clip-modal.png`, `animation-clip-preview.png`, `pathfollow-*.png`, `game-window-settings.png`, `grid-settings.png`, `viewport-grid.png`, `add-component.png`, `asset-inspector-texture.png`, `file-menu.png`, `project-scene-hud.png`). Tasks with no screenshot ship without one; **list those gaps in the final report to the owner.**

- [ ] **Step 1: Read the 7 old editor pages and the editor roadmap** before deleting them; carry over every distinct fact (shortcuts, menu paths).
- [ ] **Step 2: Implement `editor.build()`**; each task page uses numbered steps and a "Result" line.
- [ ] **Step 3: Run `python docsite/generate_site.py && python docsite/check_site.py --allow-missing engine/api/`** — Expected: no problems; no image reference to a missing file.
- [ ] **Step 4: Commit** — `docs(docsite): :memo: Rewrite the editor docs as a task-based guide`

---

### Task 8: Global search

**Files:**
- Create: `docsite/site/assets/js/search.js`; add `write_search_index()` to `docsite/_shared.py` or `generate_site.py`
- Delete: `docsite/site/assets/js/api-search.js`
- Modify: `docsite/_shared.py` (`render_page` includes `search.js` and a hidden `<dialog class="search-modal">`)

**Interfaces:**
- Produces: `assets/search-index.json` — list of `{"title","url","section","text"}` for every registered page except API (text = page prose, headings first, capped ~2 KB); `engine/api/search-index.json` (Task 6). `search.js` fetches both (an API 404 is ignored), opens on Ctrl+K / `/` / click of `[data-search-open]`, closes on Esc, arrow keys + Enter navigate results, results grouped by section. Query matching is literal tokenised substring (no `RegExp` built from user input).
- URLs in the index are site-relative; `search.js` resolves them against a `data-root` attribute on `<body>` set by `render_page`.

- [ ] **Step 1: Write tests** in `tests_shared.py`: `write_search_index` output contains an entry for a registered Page and no entry for `section="api"`; every entry has a non-empty `title` and `url`.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement index builder and `search.js`.** Handle `(`, `[`, `\` in queries and empty results ("No results" row).
- [ ] **Step 4: Run tests; check manually in the browser** (Ctrl+K, a query with `[`, a no-match query, API index absent).
- [ ] **Step 5: Commit** — `feat(docsite): :sparkles: Add Ctrl+K global search`

---

### Task 9: Merged Releases page

**Files:**
- Create: `docsite/content/releases.py`
- Modify: `docsite/site/assets/js/releases.js` (rewrite); delete `docsite/site/assets/js/editor-releases.js`, `docsite/site/releases.html` old/`editor/releases.html`

**Interfaces:**
- Produces: `releases.build()` → `releases.html` with a filter control (`All / Engine / Editor`, buttons with `aria-pressed`), a `#releases-list` container, and a fallback paragraph linking to `https://github.com/lmriccardo/a-simple-game-engine/releases` rendered server-side and hidden by JS only once releases load.
- `releases.js`: one fetch of `https://api.github.com/repos/lmriccardo/a-simple-game-engine/releases?per_page=30`. A release counts as an **Editor release** if its tag matches `/^v\d+\.\d+\.\d+e\d+\.\d+$/` (e.g. `v0.8.8e1.5`) or it has an `asge-editor-*` asset; every release is an Engine release. Cards show tag, date, badges (Engine / Editor), the sanitized markdown body (marked + DOMPurify, as today), and for editor releases the Windows (`.exe`) / Linux (`.deb`) / macOS (`.dmg`) installer links using the asset patterns from the old `editor-releases.js`. The Editor filter shows only editor releases.

- [ ] **Step 1: Write a small JS-free test** in `tests_shared.py` for the page: built HTML contains the fallback link and three filter buttons.
- [ ] **Step 2: Implement page and script.** On fetch error, non-OK status (incl. 403 rate limit) or empty array: show the fallback paragraph and an error note.
- [ ] **Step 3: Run tests and `python docsite/check_site.py --allow-missing engine/api/`** — Expected: pass; confirm in the browser with the network blocked that the fallback shows.
- [ ] **Step 4: Commit** — `feat(docsite): :sparkles: Merge engine and editor releases into one timeline`

---

### Task 10: Final pass

**Files:**
- Modify: whatever the checks flag; `CLAUDE.md` repository-layout note only if it mentions removed docsite paths.

- [ ] **Step 1: Run the full check:** `python docsite/tests_shared.py && python docsite/check_site.py --selftest && python docsite/generate_site.py && python docsite/check_site.py --allow-missing engine/api/` — Expected: all pass, no uncommitted generated drift (`git status` clean after regenerating).
- [ ] **Step 2: Grep for dead references** to `install.html`, `editor/releases.html`, `api-search.js`, `NAV_ITEMS`, `site/api/` across `docsite/`, `.github/`, `README.md`, `docs/`; fix hits.
- [ ] **Step 3: Serve locally** (`python -m http.server -d docsite/site 8000`) and hand the URL to the owner for their own review at desktop and phone widths, both themes. Report the editor screenshot gaps and that the real API output is verified only by CI.
- [ ] **Step 4: Commit** any fixes — `chore(docsite): :wrench: Fix leftover references after the refactor` (use `refactor` + `:recycle:` if logic changed).
