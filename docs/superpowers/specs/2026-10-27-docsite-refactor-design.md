# Docsite refactor — design

Branch: `refactor/docsite`. Date: 2026-10-27.

## Intent

The ASGE docsite (`docsite/`) grew page by page. This refactor gives it a
more modern, more navigable look **with the same colour palette**, and
reorganises it around what a visitor wants: understand ASGE, learn the
engine, learn the editor, look up the API, see what shipped.

Audience: C++ developers using the engine and level designers using the
editor, equally. The home page must serve both.

## Goals

- Modern, polished look; palette tokens in `docsite/site/assets/css/style.css`
  (`--accent #6a4cf0`, `--accent-2 #0f9c8c`, light and dark sets) are kept.
- Easy navigation: top bar for sections, left sidebar per section, on-page
  table of contents, Ctrl+K global search, light/dark toggle.
- Sections: **Overview**, **Engine** (Getting Started, Theory, API, Examples),
  **Editor** (task-based guide), **Releases** (one merged timeline).
- A shared template layer so layout/nav/sidebar live in one place.

## Non-goals / anti-goals

- No static-site-generator migration (MkDocs, Docusaurus, ...) and no new
  runtime dependency for the site. Python stdlib + vanilla JS only.
- No version switcher (versioned builds are out of scope).
- No redirects: old URLs (`/editor/releases.html`, `/examples/*.html`,
  `/install.html`, ...) may break.
- Do not change the palette. Do not change the Doxygen *input* (`../src`).
- Anti-goal: a pretty site whose pages are mostly empty. Theory and Editor
  pages listed below must have real content.

## Current state (what exists, verified)

- `docsite/generate_site.py` (~920 lines) renders `docsite/site/**/*.html`
  from Python data; output is **committed**. Shared helpers in
  `docsite/_shared.py` (`render_page`, `NAV_ITEMS`, `code_card`, ...).
- `docsite/generate_api.py` (~840 lines) already renders API pages from
  Doxygen **XML** (`docsite/xml/`, gitignored) into `docsite/site/api/`
  (gitignored; built in CI). So "own API pages" is the existing approach;
  the work is re-theming it to the new layout and improving its navigation,
  not starting over.
- `.github/workflows/docs.yml` already runs Doxygen (XML only,
  `GENERATE_HTML = NO`), `generate_api.py`, then assembles `docsite/site`
  plus the API artifact and deploys to Pages. It triggers on pushes to
  `main`; it needs path/trigger updates only if new inputs are added.
- Editor pages (7) with screenshots under `assets/img/editor/`; 20 example
  pages with screenshots; `releases.js` / `editor-releases.js` fetch
  releases separately for engine and editor.

## Decisions (made by the user)

- Build: keep Python, **rewrite the generator** with proper shared
  templates (layout, header, sidebar, TOC, footer partials).
- API: own generated pages from Doxygen XML.
- Structure: Install folds into Engine → Getting Started; Examples fold
  under Engine. Top-level nav: Overview, Engine, Editor, Releases (+ API
  reachable from Engine).
- Releases: one merged timeline of Engine and Editor releases with an
  Engine / Editor / All filter.
- UI: light/dark toggle, Ctrl+K global search (guide + theory + API),
  left sidebar + on-page TOC. No version switcher.
- Theory: each page = the concept, then how ASGE implements it, with links
  to API pages. **Full pages** for: ECS; Game loop & timing; Events/signals;
  Memory & threading. Other implemented subsystems (e.g. input, rendering
  primitives, audio, assets/VFS, scene/physics as implemented) get a stub
  page: title, one-paragraph summary, links to API/examples, marked "in
  progress". Content is written from `src/ASGE` and `docs/roadmap/`.
- Editor docs: restructured into a task-based guide ("Create a project",
  "Add a collider", "Animate a sprite", ...), rewritten text, existing
  screenshots reused.
- URL scheme: new, clean; no backwards compatibility.
- Work happens on the currently checked-out branch `refactor/docsite`.
- `docs.yml` may be modified to match (paths, steps).

## Decisions from the builder check (made by the user)

- Theory stubs: **only subsystems actually present in `src/ASGE` today**
  (builder verifies against the source, not CLAUDE.md, which may be stale).
  This overrides the earlier example list of stubs above.
- Editor guide: ~10 task pages covering what the 7 current pages cover,
  reusing existing screenshots. Where a task has no screenshot, ship
  without one and list the gaps for the owner.
- Examples: one page per example (migrated to the new layout) plus an
  index grid.

## Constraints

- Site output committed under `docsite/site/` except `api/` (CI-built);
  keep that split unless there is a strong reason.
- Must work on GitHub Pages as plain static files, including under a
  project sub-path (existing `rel()` helper handles relative links).
- Responsive (phone width, no horizontal page scroll); accessible basics
  (keyboard-reachable search and theme toggle, contrast in both themes).
- Search index is generated at build time (JSON) and queried client-side;
  `api-search.js` is the starting point.
- Releases come from the GitHub releases feed as today; the merged page
  must degrade gracefully if the fetch fails (link to GitHub releases).
- Repo conventions: doc-comment/commit rules in `CLAUDE.md` (gitmoji +
  Conventional Commits; scope e.g. `docsite`... use an existing scope list
  or `structure`/`workflows` as fits).

## Left to the builder

- Template mechanism (string templates / small partial functions vs.
  `string.Template`); keep stdlib-only.
- Exact sidebar grouping within Theory, Editor task list, and API
  sidebar organisation (namespace/class grouping).
- Search ranking and index shape.
- Visual details (typography, spacing, hero treatment, card styles) within
  "modern, same palette".
- How to split `generate_site.py` into modules (e.g. content data vs.
  rendering).
- Whether Editor task pages need new screenshots (reuse first; flag gaps).

## Calls made by the assistant (not yet confirmed by the owner)

- Overview page: hero with two entry cards (Engine, Editor), feature grid,
  latest release.
- URLs: `/engine/getting-started.html`, `/engine/theory/<x>.html`,
  `/engine/examples/`, `/engine/api/`, `/editor/<task>.html`, `/releases.html`.
  Moving `api/` under `engine/` means updating `generate_api.py` output
  paths and the `docs.yml` assemble step.
- `generate_api.py` shares the new template layer (no private copy).
- Search: two indexes loaded together (static one for committed pages, the
  CI-built API one); Editor and Examples are searchable too.
- Releases: distinguish engine/editor per the existing `releases.js` /
  `editor-releases.js` logic; on fetch failure show a link to GitHub releases.
- Commits: one per phase (templates, API, theory, editor, releases/search,
  workflow), all on `refactor/docsite`.
- Verification: small stdlib link-check script; API check needs local
  Doxygen, otherwise relies on CI.

## Verification

- Generator runs clean; every internal link resolves (a small link-check
  script is acceptable); API generation still works against real Doxygen
  XML.
- Manual review in a browser at desktop and phone widths, both themes.
- No engine code changes → no need to run the C++ test suite.
