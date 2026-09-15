#!/usr/bin/env python3
"""Renders docsite/site/*.html from the data in this file.

This is authoring tooling, not part of the CI/deploy pipeline: the output is
committed straight into docsite/site/ like any other static asset, so
GitHub Pages never has to run Python. Re-run this (`python
docsite/generate_site.py`) after editing EXAMPLES, the page bodies below, or
the shared nav/footer markup.
"""
import html
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SITE = ROOT / "site"
REPO = "lmriccardo/a-simple-game-engine"
GITHUB_URL = f"https://github.com/{REPO}"

NAV_ITEMS = [
    ("overview", "Overview", "index.html"),
    ("install", "Install", "install.html"),
    ("examples", "Examples", "examples/index.html"),
    ("api", "API Reference", "api/index.html"),
    ("releases", "Releases", "releases.html"),
]

# ---------------------------------------------------------------------------
# example metadata (matches examples/ on main — see examples/CMakeLists.txt)
# ---------------------------------------------------------------------------
EXAMPLES = [
    dict(
        slug="shapes_demo", title="Shapes", tagline="Every primitive ASGE can draw",
        tags=["Rendering"], kind="window", playable=False,
        description=[
            "A single screen that exercises every primitive on <code>IRenderer</code> — "
            "filled and outlined rectangles, circles, lines and points — including a line "
            "swept through a full rotation to show angle handling.",
            "There's no game logic here: <code>shapes_demo</code> exists purely as a visual "
            "reference for the rendering-primitives phase of the roadmap.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="moving_box", title="Moving Box", tagline="The smallest playable game",
        tags=["Rendering", "Input"], kind="window", playable=False,
        description=[
            "The example linked from the project README's own quick-start: a single "
            "WASD-controlled box drawn with <code>IRenderer::DrawRect</code>.",
            "It's deliberately minimal — one entity, no ECS, no assets — to show the "
            "shortest path from <code>Application&lt;TGame&gt;</code> to something on "
            "screen that responds to a player.",
        ],
        controls=[("W A S D", "Move the box")],
        source_files=["MovingBox.hpp", "MovingBox.cpp", "MovingBoxGame.cpp"],
    ),
    dict(
        slug="texture_demo", title="Textures", tagline="Loading and drawing images",
        tags=["Rendering", "Assets"], kind="window", playable=False,
        description=[
            "Loads a checker-pattern texture and a frame texture from disk through the "
            "asset pipeline, then draws them with <code>IRenderer::DrawTexture</code> and "
            "<code>DrawTextureAffine</code> — the latter continuously rotated to demonstrate "
            "affine (rotated/scaled) texture drawing rather than plain axis-aligned blits.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="text_demo", title="Text Rendering", tagline="Fonts, sizes and color",
        tags=["Rendering", "Assets"], kind="window", playable=False,
        description=[
            "Bakes the same font at three different pixel heights — each ASGE "
            "<code>Font</code> is rasterized at one fixed size, so multiple on-screen "
            "sizes means loading it multiple times, each with its own atlas texture.",
            "A headline drawn alongside them continuously cycles through hue space, "
            "exercising per-draw color tinting on top of the baked glyph atlas.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="animation_demo", title="Sprite Animation", tagline="Spritesheets via a shared frame table",
        tags=["Rendering", "ECS", "Assets"], kind="window", playable=False,
        description=[
            "Showcase for <code>components::Animation</code> and "
            "<code>systems::AnimationSystem</code>/<code>RenderPipeline</code>: playing back "
            "a spritesheet through per-entity frame state, with the frame list itself loaded "
            "as a shared <code>asset::FrameTable</code> (<code>walk.toml</code>) rather than "
            "embedded per-entity.",
        ],
        controls=[
            ("Left Click", "Spawn another animated sprite at the cursor"),
            ("SPACE", "Play/pause every sprite's animation"),
            ("R", "Reset"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="ecs_demo", title="ECS", tagline="Entities, components and systems in motion",
        tags=["ECS", "Rendering"], kind="window", playable=False,
        description=[
            "A player entity and a handful of sprite entities, all built through the ECS "
            "<code>Registry</code> rather than hand-written classes.",
            "Sprite/texture attachment is deferred to the first <code>Render()</code> call, "
            "since a live <code>IRenderer</code> doesn't exist yet when <code>IGame</code> is "
            "constructed — a pattern every sprite-driven example after this one follows.",
        ],
        controls=[("W A S D", "Move the player entity")],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="input_demo", title="Input System", tagline="Polling InputState end-to-end",
        tags=["Input"], kind="window", playable=False,
        description=[
            "Step 3 of the input-system roadmap: a small <code>InputState</code>/"
            "<code>InputSystem</code> showcase.",
            "Everything here is driven by polling <code>InputState</code> inside "
            "<code>Update()</code> — movement, an edge-triggered toggle, a mouse cursor, "
            "held vs. edge-triggered mouse buttons, and scroll — with "
            "<code>OnSystemEvent</code> left completely empty, proving the polling API is "
            "enough on its own for a real game loop.",
        ],
        controls=[
            ("W A S D", "Move the box"),
            ("SPACE", "Toggle light/dark background (edge-triggered)"),
            ("Left Click (hold)", "Shown as held while down"),
            ("Right Click", "Drop a mark at the cursor (edge-triggered)"),
            ("Scroll", "Shown live"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="scene_demo", title="Scene Management", tagline="A whole level loaded from TOML",
        tags=["Scenes", "ECS", "Assets"], kind="window", playable=False,
        description=[
            "Loads its whole entity set from a hand-authored TOML scene file "
            "(<code>assets/scene.toml</code>) via <code>SceneManager::LoadScene</code> "
            "instead of spawning entities in code — see <code>ecs_demo</code> for the "
            "code-driven equivalent this replaces.",
            "Ties together every ASGE subsystem at its current state: the virtual "
            "filesystem loads the TOML, the asset pipeline resolves textures, the ECS "
            "registry holds the resulting entities, and <code>SceneManager</code> can swap "
            "the whole active scene at runtime or snapshot it back out to disk.",
        ],
        controls=[
            ("W A S D", "Move the player entity"),
            ("P", "Save a snapshot of the current scene back to TOML (edge-triggered)"),
            ("L", "Swap to the other scene file, alternating back and forth"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="physics_demo", title="Physics", tagline="Gravity, collision response and triggers",
        tags=["Physics", "ECS"], kind="window", playable=False,
        description=[
            "Showcase for <code>Rigidbody</code>, gravity, mass-weighted collision "
            "response, and Trigger colliders, all driven through "
            "<code>systems::PhysicsUpdate</code>.",
            "A floor and two side walls are static Solid Colliders — deliberately no "
            "<code>Velocity</code>/<code>Rigidbody</code>, so collision response treats "
            "them as immovable. A yellow-outlined zone acts as a Trigger, reporting overlap "
            "without any physical push-back.",
        ],
        controls=[("Click", "Drop a new box at the cursor"), ("R", "Reset the scene")],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="audio_demo", title="Audio", tagline="One-shot and looping playback",
        tags=["Audio"], kind="window", playable=False,
        description=[
            "Demonstrates <code>AudioSource</code>/<code>AudioSystem</code>: replaying a "
            "short one-shot \"blip\" immediately even while it's still sounding, toggling a "
            "looping ambient hum, adjusting the audio device's master gain, and detaching a "
            "source's stream back to the shared pool.",
        ],
        controls=[
            ("SPACE", "Replay the one-shot blip"),
            ("L", "Toggle the looping ambient hum"),
            ("↑ / ↓", "Master volume"),
            ("D", "Detach the blip's stream back to the pool"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="background_changer", title="Background Changer", tagline="About the smallest ASGE program",
        tags=["Rendering", "Getting Started"], kind="window", playable=False,
        description=[
            "About as small as an ASGE program gets: one component holding an RGBA color, "
            "cleared to the screen every frame, randomized on a keypress.",
            "A good first file to read before anything involving ECS or the asset pipeline.",
        ],
        controls=[("SPACE", "Randomize the background color")],
        source_files=["Game.hpp", "Game.cpp", "Component.hpp"],
    ),
    dict(
        slug="configuration_reader", title="Configuration Reader",
        tagline="Reading typed values out of a TOML config",
        tags=["Configuration"], kind="console", playable=False,
        description=[
            "Opens a TOML configuration file through <code>ConfigurationManager</code> and "
            "reads back typed settings (<code>Get&lt;int&gt;</code>, etc.) with explicit "
            "error handling on missing or mistyped keys, printing each to stdout.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="configuration_writer", title="Configuration Writer",
        tagline="Building a TOML document from code",
        tags=["Configuration"], kind="console", playable=False,
        description=[
            "The write-side counterpart to <code>configuration_reader</code>: assembles a "
            "TOML document in memory via <code>toml::TOML_Builder</code>, then reads it "
            "straight back through <code>ConfigurationManager</code> to confirm the "
            "round-trip.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="file_watching", title="File Watching", tagline="Filesystem change notifications",
        tags=["Filesystem"], kind="console", playable=False,
        description=[
            "Starts a <code>FileWatcher</code> on a directory and prints every "
            "<code>FileEvent</code> (created/modified/removed) it receives, cancelling "
            "itself on the first event — a minimal example of ASGE's cross-platform "
            "filesystem-watching API.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="image_info", title="Image Info", tagline="Loading an image and inspecting its pixels",
        tags=["Assets", "Media"], kind="console", playable=False,
        description=[
            "Loads a bitmap through <code>Image::Load</code> and prints its dimensions and "
            "raw pixel data — the smallest possible use of the image-loading half of the "
            "asset pipeline, with no rendering involved at all.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="scoped_profiler", title="Scoped Profiler", tagline="Nested scope timing",
        tags=["Profiling", "Tooling"], kind="console", playable=False,
        description=[
            "Times nested code paths with <code>PROFILE_SCOPE</code>, showing that scopes "
            "can nest inside each other the same way a real per-frame <code>Update()</code> "
            "would, then prints the resulting timing tree.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
]

EXAMPLES_BY_SLUG = {e["slug"]: e for e in EXAMPLES}

# ---------------------------------------------------------------------------
# page shell
# ---------------------------------------------------------------------------


def rel(depth: int, path: str) -> str:
    return ("../" * depth) + path


def render_nav(active: str, depth: int) -> str:
    links = []
    for key, label, path in NAV_ITEMS:
        cls = ' class="active"' if key == active else ""
        links.append(f'<a href="{rel(depth, path)}"{cls}>{label}</a>')
    links.append(f'<a href="{GITHUB_URL}" target="_blank" rel="noopener">GitHub</a>')
    return "\n    ".join(links)


THEME_ICON = (
    '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" '
    'stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" '
    'aria-hidden="true"><circle cx="12" cy="12" r="4"></circle>'
    '<path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2'
    'M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>'
)

MENU_ICON = (
    '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" '
    'stroke-width="2" stroke-linecap="round" aria-hidden="true">'
    '<path d="M4 7h16M4 12h16M4 17h16"/></svg>'
)


def render_page(*, title, description, active, depth, body, extra_head="", extra_scripts=""):
    nav = render_nav(active, depth)
    css = rel(depth, "assets/css/style.css")
    logo = rel(depth, "assets/img/logo.svg")
    favicon = rel(depth, "assets/img/favicon.svg")
    main_js = rel(depth, "assets/js/main.js")
    home = rel(depth, "index.html")
    canonical_desc = html.escape(description)

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{html.escape(title)} · ASGE</title>
<meta name="description" content="{canonical_desc}"/>
<meta property="og:title" content="{html.escape(title)} · ASGE"/>
<meta property="og:description" content="{canonical_desc}"/>
<link rel="icon" type="image/svg+xml" href="{favicon}"/>
<link rel="stylesheet" href="{css}"/>
{extra_head}
</head>
<body>
<header class="site-header">
  <div class="container">
    <a class="brand" href="{home}"><img src="{logo}" alt=""/> ASGE</a>
    <nav class="nav-links">
    {nav}
    </nav>
    <div class="nav-right">
      <button class="icon-btn" data-theme-toggle title="Toggle color theme" aria-label="Toggle color theme">{THEME_ICON}</button>
      <button class="icon-btn nav-toggle" data-nav-toggle title="Menu" aria-label="Toggle navigation menu">{MENU_ICON}</button>
    </div>
  </div>
</header>
<main>
{body}
</main>
<footer class="site-footer">
  <div class="container">
    <div>&#169; ASGE — A Simple Game Engine. MIT Licensed.</div>
    <div class="footer-links">
      <a href="{GITHUB_URL}" target="_blank" rel="noopener">GitHub</a>
      <a href="{GITHUB_URL}/issues" target="_blank" rel="noopener">Issues</a>
      <a href="{rel(depth, 'index.html')}#roadmap">Roadmap</a>
      <a href="{rel(depth, 'releases.html')}">Releases</a>
    </div>
  </div>
</footer>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.10.0/highlight.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.10.0/languages/cpp.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.10.0/languages/cmake.min.js"></script>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.10.0/styles/atom-one-dark.min.css" media="(prefers-color-scheme: dark)"/>
<script src="{main_js}"></script>
{extra_scripts}
</body>
</html>
"""


def write(path: Path, content: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    print(f"wrote {path.relative_to(ROOT)}")


def code_card(label: str, lang: str, code: str) -> str:
    return f"""<div class="code-card">
  <div class="code-card-head"><span>{html.escape(label)}</span><button class="copy-btn" data-copy>Copy</button></div>
  <pre><code class="language-{lang}">{html.escape(code)}</code></pre>
</div>"""


# ---------------------------------------------------------------------------
# page: overview (index.html)
# ---------------------------------------------------------------------------

FEATURES = [
    ("Performance", "Thin abstractions over SDL3, custom allocators, and a thread pool for concurrent work — the engine tries to stay out of your frame budget's way."),
    ("Simplicity", "Small, composable subsystems instead of one monolithic framework. Pull in what a game needs through the <code>ASGE.hpp</code> umbrella header."),
    ("Extensibility", "An ECS-based gameplay layer plus a signal/event system (<code>asge::signals::Signal</code>) for decoupling systems from each other."),
    ("Tooling", "A debug logger, timing/profiling utilities (<code>ScopedTimer</code>), a virtual filesystem, and a growing set of runnable examples."),
]

ROADMAP = [
    ("Tier 0 — Foundation", [
        ("Core Engine", "done", "phase-1/01.1-core-engine.md"),
        ("Rendering Primitives", "done", "phase-1/02.1-rendering-primitives.md"),
    ]),
    ("Tier 1 — MVP-critical", [
        ("Entity-Component-System", "done", "phase-1/03.1-entity-component-system.md"),
        ("Input System", "done", "phase-1/04.1-input-system.md"),
        ("Asset Pipeline", "done", "phase-1/05.1-asset-pipeline.md"),
        ("Scene Management", "done", "phase-1/06.1-scene-management.md"),
    ]),
    ("Tier 2 — Makes it feel like a game", [
        ("Physics", "done", "phase-1/07.1-physics-system.md"),
        ("Audio", "done", "phase-1/08.1-audio-system.md"),
        ("UI Framework", "progress", "phase-1/09.1-ui-framework.md"),
    ]),
    ("Tier 3 — Revisit once Tier 1-2 exist", [
        ("Rendering System (2D camera, compositing)", "progress", "phase-1/10.1-rendering-system.md"),
        ("Scripting & Events", "todo", "phase-2/11.2-scripting-and-events.md"),
    ]),
    ("Tier 4 — Explicitly deferred", [
        ("Networking", "todo", "phase-2/12.2-networking.md"),
        ("Optimization", "progress", "phase-1/13.1-optimization.md"),
        ("Documentation", "progress", "phase-1/14.1-documentation.md"),
    ]),
]

BADGE_LABEL = {"done": "Done", "progress": "In progress", "todo": "Planned"}


def build_index():
    features_html = "\n".join(
        f'<div class="card"><h3>{title}</h3><p>{desc}</p></div>'
        for title, desc in FEATURES
    )

    roadmap_html = ""
    for tier_name, items in ROADMAP:
        roadmap_html += f'<h3 style="margin-top:28px;">{html.escape(tier_name)}</h3><div class="roadmap-list">'
        for name, status, doc_path in items:
            badge = f'<span class="badge badge-{status}">{BADGE_LABEL[status]}</span>'
            link = f"{GITHUB_URL}/blob/main/docs/roadmap/{doc_path}"
            roadmap_html += (
                f'<div class="roadmap-item"><span class="name">{html.escape(name)}</span>'
                f'{badge}<a href="{link}" target="_blank" rel="noopener" class="tag" style="margin-left:8px;">docs →</a></div>'
            )
        roadmap_html += "</div>"

    example_tiles = "\n".join(
        f'<a class="tag" href="examples/{e["slug"]}.html" style="padding:6px 12px;">{e["title"]}</a>'
        for e in EXAMPLES[:8]
    )

    body = f"""
<section class="hero">
  <div class="container">
    <span class="kicker">v0.8.0 &middot; MIT Licensed &middot; C++23</span>
    <h1>A Simple Game Engine, built to stay out of your way.</h1>
    <p class="lede">ASGE is a modular, lightweight, cross-platform 2D game engine written in modern
    C++ (C++23) on top of <a href="https://www.libsdl.org/" target="_blank" rel="noopener">SDL3</a>.
    Small composable subsystems, an ECS-based gameplay layer, and a growing set of runnable
    examples — targeting Windows, Linux and macOS.</p>
    <div class="btn-row">
      <a class="btn btn-primary" href="install.html">Get started</a>
      <a class="btn btn-ghost" href="examples/index.html">Browse examples</a>
      <a class="btn btn-ghost" href="api/index.html">API reference</a>
      <a class="btn btn-ghost" href="{GITHUB_URL}" target="_blank" rel="noopener">View on GitHub</a>
    </div>
    <div class="hero-stats">
      <div class="stat"><b>{len(EXAMPLES)}</b><span>runnable examples</span></div>
      <div class="stat"><b>C++23</b><span>modern, header-driven API</span></div>
      <div class="stat"><b>3</b><span>target platforms</span></div>
      <div class="stat"><b>SDL3</b><span>current render backend</span></div>
    </div>
  </div>
</section>

<section>
  <div class="container">
    <h2>Built around four ideas</h2>
    <div class="grid" style="margin-top:20px;">
      {features_html}
    </div>
  </div>
</section>

<section class="alt">
  <div class="container">
    <h2>Repository layout</h2>
    <table>
      <tr><th>Path</th><th>Contents</th></tr>
      <tr><td><code>src/ASGE/</code></td><td>Engine source — Application, Core, Events, Game, Input, Video, Audio</td></tr>
      <tr><td><code>examples/</code></td><td>Small standalone programs demonstrating each subsystem</td></tr>
      <tr><td><code>tests/</code></td><td>GoogleTest unit test suite</td></tr>
      <tr><td><code>docs/roadmap/</code></td><td>Design docs and the phased implementation roadmap</td></tr>
      <tr><td><code>scripts/</code></td><td>Dependency-fetching scripts (SDL3, stb) for each platform</td></tr>
      <tr><td><code>cmake/</code></td><td>CMake package config used when ASGE is installed</td></tr>
    </table>
  </div>
</section>

<section id="roadmap">
  <div class="container">
    <h2>Roadmap status</h2>
    <p class="lede">ASGE is under active development, worked through a phased
    <a href="{GITHUB_URL}/blob/main/docs/roadmap/README.md" target="_blank" rel="noopener">roadmap</a>.
    Tiers are priority order; each system also has its own phase docs for scope/maturity.</p>
    {roadmap_html}
  </div>
</section>

<section class="alt">
  <div class="container">
    <h2>Jump into an example</h2>
    <p class="lede">Every example ships with real screenshots and its full embedded source.</p>
    <div style="display:flex;flex-wrap:wrap;gap:10px;margin-top:16px;">
      {example_tiles}
      <a class="tag" href="examples/index.html" style="padding:6px 12px;">All {len(EXAMPLES)} examples →</a>
    </div>
  </div>
</section>

<section>
  <div class="container">
    <h2>Minimal game</h2>
    <p class="lede">A <code>Game&lt;TStateId&gt;</code> subclass drives a stack of
    <code>IGameState</code>s — <code>TStateId</code> is whatever identifies a screen. See
    <a href="examples/moving_box.html">Moving Box</a> for the complete, playable version of this.</p>
    {code_card("main.cpp", "cpp", MINIMAL_GAME_SNIPPET)}
  </div>
</section>
"""
    return render_page(
        title="A Simple Game Engine",
        description="ASGE is a modular, lightweight, cross-platform 2D game engine written in modern C++ on top of SDL3.",
        active="overview", depth=0, body=body,
    )


MINIMAL_GAME_SNIPPET = """#include <ASGE/ASGE.hpp>

class MyGameState : public asge::game::state::IGameState<int>
{
public:
    std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override
    {
        // Game logic goes here
        return std::nullopt; // engaged instead: push/pop/replace to another state
    }

    void Render(asge::video::IRenderer& inRenderer) override
    {
        // Draw calls go here
    }

    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override
    {
        // Handle window/system events
    }
};

class MyGame : public asge::game::Game<int>
{
public:
    explicit MyGame(asge::video::IRenderer& inRenderer) : Game(inRenderer)
    {
        SetInitialState(0);
    }

protected:
    std::unique_ptr<StateType> CreateState(int inId) override
    {
        return std::make_unique<MyGameState>();
    }
};

int main()
{
    asge::Application<MyGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
"""

# ---------------------------------------------------------------------------
# page: install.html
# ---------------------------------------------------------------------------


def platform_tabs(tab_id: str, panels: list[tuple[str, str, str]]) -> str:
    """panels: list of (tab label, lang, code)"""
    buttons = "\n".join(
        f'<button class="tab-btn{" active" if i == 0 else ""}" data-tab="{tab_id}-{i}">{label}</button>'
        for i, (label, _, _) in enumerate(panels)
    )
    body = "\n".join(
        f'<div class="tab-panel{" active" if i == 0 else ""}" data-panel="{tab_id}-{i}">'
        f'{code_card(label, lang, code)}</div>'
        for i, (label, lang, code) in enumerate(panels)
    )
    return f'<div data-tabs><div class="tabs">{buttons}</div>{body}</div>'


OPTION1_SNIPPET = """add_subdirectory(third-party/asge)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE ASGE)

# On Windows, my_game.exe also needs SDL3.dll next to it at runtime
asge_copy_sdl3_runtime(my_game)"""

OPTION2_SNIPPET = """include(FetchContent)
FetchContent_Declare(
    asge
    GIT_REPOSITORY https://github.com/lmriccardo/a-simple-game-engine.git
    GIT_TAG main
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/third-party/asge"
)
FetchContent_MakeAvailable(asge)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE ASGE)
asge_copy_sdl3_runtime(my_game)"""

OPTION3_SNIPPET = """find_package(ASGE REQUIRED CONFIG)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE ASGE::ASGE)"""


CLONE_SNIPPET = "git clone https://github.com/lmriccardo/a-simple-game-engine.git asge\ncd asge"


def build_install():
    deps_tabs = platform_tabs("deps", [
        ("Windows (PowerShell)", "powershell", "./scripts/install-deps.ps1"),
        ("Linux / macOS", "bash", "./scripts/install-deps.sh"),
    ])
    build_tabs = platform_tabs("build", [
        ("Windows", "bash", "cmake --preset windows\ncmake --build --preset windows\nctest --preset windows-debug"),
        ("Linux", "bash", "cmake --preset linux\ncmake --build --preset linux\nctest --preset linux-debug"),
        ("macOS", "bash", "cmake --preset macos\ncmake --build --preset macos\nctest --preset macos-debug"),
    ])

    body = f"""
<section class="hero" style="padding:56px 0 40px;">
  <div class="container">
    <span class="kicker">Installation</span>
    <h1>Build ASGE from source</h1>
    <p class="lede">ASGE builds a static library target and installs a CMake package config, so it
    can be consumed as a subdirectory, via <code>FetchContent</code>, or as an installed package.</p>
  </div>
</section>

<section style="padding-top:40px;">
  <div class="container">
    <h2>Prerequisites</h2>
    <ul>
      <li>A C++23 compiler — MSVC on Windows, GCC/Clang on Linux, Clang on macOS</li>
      <li><a href="https://cmake.org/" target="_blank" rel="noopener">CMake</a> &ge; 3.31</li>
      <li>Git</li>
    </ul>

    <ol class="steps">
      <li>
        <h3>Clone the repository</h3>
        {code_card("shell", "bash", CLONE_SNIPPET)}
      </li>
      <li>
        <h3>Fetch vendored dependencies</h3>
        <p>SDL3 and the stb single-header libraries aren't pulled in by CMake — a script vendors them per platform.</p>
        {deps_tabs}
      </li>
      <li>
        <h3>Configure, build and test</h3>
        <p>Using one of the provided <a href="{GITHUB_URL}/blob/main/CMakePresets.json" target="_blank" rel="noopener">CMake presets</a>
        (also buildable without a preset via plain <code>cmake -S . -B build</code>):</p>
        {build_tabs}
        <p class="lede">Built examples land in <code>bin/</code>.</p>
      </li>
    </ol>
  </div>
</section>

<section class="alt">
  <div class="container">
    <h2>Using ASGE in your own C++ project</h2>
    <p class="lede">Either way, pull in the whole engine through the umbrella header:
    {code_card("", "cpp", "#include <ASGE/ASGE.hpp>")}</p>

    <h3>Option 1 — add_subdirectory</h3>
    <p>Vendor ASGE into your project (e.g. as a submodule under <code>third-party/asge</code>) and add it directly:</p>
    {code_card("CMakeLists.txt", "cmake", OPTION1_SNIPPET)}

    <h3>Option 2 — FetchContent</h3>
    {code_card("CMakeLists.txt", "cmake", OPTION2_SNIPPET)}
    <p class="lede">ASGE's own dependencies aren't fetched by CMake, so the <strong>first</strong>
    <code>FetchContent_MakeAvailable</code> call fails — it clones ASGE, then immediately tries to
    configure it before <code>find_package(SDL3)</code> can find anything. Run ASGE's install
    script against the freshly-cloned copy once, then reconfigure:</p>
    {code_card("shell", "bash", "./third-party/asge/scripts/install-deps.sh      # or install-deps.ps1 on Windows")}

    <h3>Option 3 — installed package</h3>
    <p>Install ASGE (<code>cmake --install build</code>), then from your own project:</p>
    {code_card("CMakeLists.txt", "cmake", OPTION3_SNIPPET)}
  </div>
</section>

<section>
  <div class="container">
    <h2>Testing</h2>
    <p class="lede">Unit tests (GoogleTest) live under <code>tests/</code> and build when
    <code>BUILD_TESTING</code> is <code>ON</code>:</p>
    {code_card("shell", "bash", "ctest --preset windows-debug   # or the linux / macos equivalent")}
  </div>
</section>
"""
    return render_page(
        title="Installation",
        description="Build ASGE from source, or consume it from your own C++ project via add_subdirectory, FetchContent, or an installed CMake package.",
        active="install", depth=0, body=body,
    )


# ---------------------------------------------------------------------------
# page: examples/index.html
# ---------------------------------------------------------------------------


def example_card(e: dict) -> str:
    img_path = f"assets/img/examples/{e['slug']}.png"
    has_img = (SITE / img_path).exists()
    thumb = (
        f'<img src="../{img_path}" alt="{html.escape(e["title"])} screenshot" loading="lazy"/>'
        if has_img else
        f'<div class="noshot">console example<br/>(no window)</div>'
    )
    badge = '<span class="playable-badge">Play in browser</span>' if e.get("playable") else ""
    tags = "".join(f'<span class="tag">{html.escape(t)}</span>' for t in e["tags"])
    return f"""<a class="example-card" href="{e['slug']}.html">
  <div class="thumb">{thumb}{badge}</div>
  <div class="body">
    <h3>{html.escape(e['title'])}</h3>
    <p>{html.escape(e['tagline'])}</p>
    <div class="tags">{tags}</div>
  </div>
</a>"""


def build_examples_index():
    cards = "\n".join(example_card(e) for e in EXAMPLES)
    body = f"""
<section class="hero" style="padding:56px 0 40px;">
  <div class="container">
    <span class="kicker">Examples</span>
    <h1>{len(EXAMPLES)} runnable examples</h1>
    <p class="lede">Every subsystem ships with a small standalone program under
    <code>examples/</code>, built alongside the engine when <code>ASGE_BUILD_EXAMPLES</code> is
    <code>ON</code>. Each one below has real captured screenshots (or terminal output, for the
    console-only ones) and its full source inline.</p>
  </div>
</section>
<section style="padding-top:32px;">
  <div class="container">
    <div class="grid">
      {cards}
    </div>
  </div>
</section>
"""
    return render_page(
        title="Examples",
        description=f"{len(EXAMPLES)} runnable ASGE examples covering rendering, ECS, input, physics, audio, scenes and more.",
        active="examples", depth=1, body=body,
    )


# ---------------------------------------------------------------------------
# page: examples/<slug>.html
# ---------------------------------------------------------------------------

LANG_BY_EXT = {".hpp": "cpp", ".h": "cpp", ".cpp": "cpp", ".cc": "cpp"}


def read_example_source(slug: str, filename: str) -> str | None:
    path = ROOT.parent / "examples" / slug / filename
    if not path.exists():
        return None
    return path.read_text(encoding="utf-8")


def build_example_detail(e: dict):
    slug = e["slug"]
    img_path = f"assets/img/examples/{slug}.png"
    has_img = (SITE / img_path).exists()
    tags = "".join(f'<span class="tag">{html.escape(t)}</span>' for t in e["tags"])
    desc_html = "\n".join(f"<p>{p}</p>" for p in e["description"])

    if e["kind"] == "window":
        media = (
            f'<img src="../{img_path}" alt="{html.escape(e["title"])} screenshot"/>'
            if has_img else
            '<div style="display:flex;align-items:center;justify-content:center;height:100%;color:var(--text-muted);">screenshot pending</div>'
        )
        side_extra = ""
        if e.get("controls"):
            rows = "".join(
                f'<li><kbd>{html.escape(k)}</kbd> <span>{html.escape(d)}</span></li>'
                for k, d in e["controls"]
            )
            side_extra = f'<h3>Controls</h3><ul class="controls-list">{rows}</ul>'
        else:
            side_extra = '<p class="lede">No input — purely a visual reference for this subsystem.</p>'
    else:
        media = '<div style="display:flex;align-items:center;justify-content:center;height:100%;color:var(--text-muted);padding:20px;text-align:center;">console example — no window, see captured output below</div>'
        side_extra = ""

    playable_section = ""
    if e.get("playable"):
        playable_section = f"""
    <div id="play-wrap" hidden>
      <div class="play-note">🎮 This example also has an experimental WebAssembly build — click below to play it right in this page.</div>
      <button class="btn btn-primary" id="play-btn" data-target="../playable/{slug}/index.html">Play {html.escape(e['title'])} in your browser</button>
      <div id="play-frame-wrap" hidden style="margin-top:16px;" class="example-media">
        <iframe id="play-frame" title="{html.escape(e['title'])} (playable build)" allow="autoplay"></iframe>
      </div>
    </div>
    <script>
      (function(){{
        var url = "../playable/{slug}/index.html";
        fetch(url, {{method: "HEAD"}}).then(function(r){{
          if (r.ok) document.getElementById("play-wrap").hidden = false;
        }}).catch(function(){{}});
        document.addEventListener("DOMContentLoaded", function(){{
          var btn = document.getElementById("play-btn");
          if (!btn) return;
          btn.addEventListener("click", function(){{
            var wrap = document.getElementById("play-frame-wrap");
            document.getElementById("play-frame").src = btn.getAttribute("data-target");
            wrap.hidden = false;
            btn.hidden = true;
          }});
        }});
      }})();
    </script>"""

    outfile = None
    sample_output = None
    if e["kind"] == "console":
        outfile = SITE / "assets" / "console-output" / f"{slug}.txt"
        if outfile.exists():
            sample_output = outfile.read_text(encoding="utf-8")

    console_section = ""
    if e["kind"] == "console" and sample_output:
        console_section = f"""
<section>
  <div class="container">
    <h2>Sample output</h2>
    {code_card(f"{slug} output", "bash", sample_output)}
  </div>
</section>"""

    source_cards = []
    for filename in e["source_files"]:
        src = read_example_source(slug, filename)
        if src is None:
            continue
        source_cards.append(code_card(f"examples/{slug}/{filename}", "cpp", src))
    source_html = "\n".join(source_cards)

    cmake_target = code_card("build just this example", "bash",
                              f"cmake --build --preset windows-debug --target {slug}\n"
                              f"# binary lands under bin/ (e.g. bin/Debug/{slug}.exe on Windows)")

    body = f"""
<section style="padding-top:36px;padding-bottom:20px;">
  <div class="container">
    <a class="back-link" href="index.html">&larr; All examples</a>
    <div class="example-hero">
      <div>
        <span class="kicker">{" &middot; ".join(html.escape(t) for t in e['tags'])}</span>
        <h1>{html.escape(e['title'])}</h1>
        <p class="lede">{html.escape(e['tagline'])}</p>
        {desc_html}
        {side_extra}
        {playable_section}
        <div class="tags" style="margin-top:16px;">{tags}</div>
      </div>
      <div class="example-media">{media}</div>
    </div>
  </div>
</section>
{console_section}
<section class="alt">
  <div class="container">
    <h2>Run it yourself</h2>
    {cmake_target}
    <p class="lede">See <a href="../install.html">Installation</a> for the full build setup.</p>
  </div>
</section>
<section>
  <div class="container">
    <h2>Source</h2>
    {source_html}
    <p><a href="{GITHUB_URL}/tree/main/examples/{slug}" target="_blank" rel="noopener">Browse the full example on GitHub →</a></p>
  </div>
</section>
"""
    return render_page(
        title=e["title"],
        description=e["tagline"],
        active="examples", depth=1, body=body,
    )


# ---------------------------------------------------------------------------
# page: releases.html
# ---------------------------------------------------------------------------


def build_releases():
    body = f"""
<section class="hero" style="padding:56px 0 40px;">
  <div class="container">
    <span class="kicker">Releases</span>
    <h1>Releases</h1>
    <p class="lede">Packaged builds for Windows, Linux and macOS, generated automatically on every
    <code>v*.*.*</code> tag. Pulled live from the
    <a href="{GITHUB_URL}/releases" target="_blank" rel="noopener">GitHub Releases API</a> —
    if it doesn't load, browse them directly on GitHub instead.</p>
    <div class="btn-row">
      <a class="btn btn-primary" href="{GITHUB_URL}/releases" target="_blank" rel="noopener">All releases on GitHub</a>
      <a class="btn btn-ghost" href="{GITHUB_URL}/blob/main/CHANGELOG.md" target="_blank" rel="noopener">Full CHANGELOG.md</a>
    </div>
  </div>
</section>
<section>
  <div class="container">
    <div id="releases-list"></div>
  </div>
</section>
"""
    extra_scripts = (
        '<script src="https://cdnjs.cloudflare.com/ajax/libs/marked/13.0.2/marked.min.js"></script>\n'
        '<script src="https://cdnjs.cloudflare.com/ajax/libs/dompurify/3.1.6/purify.min.js"></script>\n'
        '<script src="assets/js/releases.js"></script>'
    )
    return render_page(
        title="Releases",
        description="Packaged ASGE builds for Windows, Linux and macOS, generated automatically on every tagged release.",
        active="releases", depth=0, body=body, extra_scripts=extra_scripts,
    )


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def main():
    write(SITE / "index.html", build_index())
    write(SITE / "install.html", build_install())
    write(SITE / "examples" / "index.html", build_examples_index())
    for e in EXAMPLES:
        write(SITE / "examples" / f"{e['slug']}.html", build_example_detail(e))
    write(SITE / "releases.html", build_releases())


if __name__ == "__main__":
    main()
