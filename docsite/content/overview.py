"""Overview (home) page."""
import html

from _shared import GITHUB_URL, Page, code_card, register, render_page
from content.examples_data import EXAMPLES

PAGE = register(Page(
    "index.html", "A Simple Game Engine", "overview", "",
    "ASGE is a modular, lightweight, cross-platform 2D game engine written in modern C++ on top of SDL3.",
))

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
        f'<a class="tag" href="engine/examples/{e["slug"]}.html" style="padding:6px 12px;">{e["title"]}</a>'
        for e in EXAMPLES[:8]
    )

    body = f"""
<section class="hero">
  <div class="container">
    <span class="kicker">MIT Licensed &middot; C++23 &middot; SDL3</span>
    <h1>A Simple Game Engine, built to stay out of your way.</h1>
    <p class="lede">ASGE is a modular, lightweight, cross-platform 2D game engine written in modern
    C++ (C++23) on top of <a href="https://www.libsdl.org/" target="_blank" rel="noopener">SDL3</a>.
    Small composable subsystems, an ECS-based gameplay layer, and a growing set of runnable
    examples — targeting Windows, Linux and macOS.</p>
    <div class="btn-row">
      <a class="btn btn-primary" href="engine/getting-started.html">Get started</a>
      <a class="btn btn-ghost" href="engine/examples/index.html">Browse examples</a>
      <a class="btn btn-ghost" href="engine/api/index.html">API reference</a>
      <a class="btn btn-ghost" href="{GITHUB_URL}" target="_blank" rel="noopener">View on GitHub</a>
    </div>
    <p id="latest-release" class="lede" hidden></p>
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
    <div class="grid">
      <a class="card" href="engine/getting-started.html"><h3>Engine</h3><p>Install ASGE, learn how it works, browse the API reference and runnable examples.</p></a>
      <a class="card" href="editor/index.html"><h3>Level editor</h3><p>Build projects, scenes, colliders and animations visually, then run them on the engine.</p></a>
    </div>
  </div>
</section>

<section class="alt">
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
      <a class="tag" href="engine/examples/index.html" style="padding:6px 12px;">All {len(EXAMPLES)} examples →</a>
    </div>
  </div>
</section>

<section>
  <div class="container">
    <h2>Minimal game</h2>
    <p class="lede">A <code>Game&lt;TStateId&gt;</code> subclass drives a stack of
    <code>IGameState</code>s — <code>TStateId</code> is whatever identifies a screen. See
    <a href="engine/examples/moving_box.html">Moving Box</a> for the complete, playable version of this.</p>
    {code_card("main.cpp", "cpp", MINIMAL_GAME_SNIPPET)}
  </div>
</section>
"""
    return render_page(page=PAGE, body=body, raw=True, extra_scripts='<script src="assets/js/releases.js"></script>')


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


def build() -> dict[str, str]:
    return {"index.html": build_index()}
