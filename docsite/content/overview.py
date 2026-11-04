"""Overview (home) page."""
import html

from _shared import GITHUB_URL, SITE, Page, code_card, register, render_page
from content.examples_data import EXAMPLES

PAGE = register(Page(
    "index.html", "A Simple Game Engine", "overview", "",
    "ASGE is a modular, lightweight, cross-platform 2D game engine written in modern C++ on top of SDL3.",
))

FEATURES = [
    ("Performance", "Thin abstractions over SDL3, custom allocators, and a thread pool for concurrent work. It stays out of your frame budget’s way."),
    ("Simplicity", "Small, composable subsystems instead of one monolithic framework. Pull in what a game needs through the <code>ASGE.hpp</code> umbrella header."),
    ("Extensibility", "An ECS-based gameplay layer plus a signal/event system (<code>asge::signals::Signal</code>) for decoupling systems from each other."),
    ("Tooling", "A debug logger, timing/profiling utilities (<code>ScopedTimer</code>), a virtual filesystem, and a growing set of runnable examples."),
]

ROADMAP = [
    ("Tier 0: Foundation", [
        ("Core Engine", "done", "phase-1/01.1-core-engine.md"),
        ("Rendering Primitives", "done", "phase-1/02.1-rendering-primitives.md"),
    ]),
    ("Tier 1: MVP-critical", [
        ("Entity-Component-System", "done", "phase-1/03.1-entity-component-system.md"),
        ("Input System", "done", "phase-1/04.1-input-system.md"),
        ("Asset Pipeline", "done", "phase-1/05.1-asset-pipeline.md"),
        ("Scene Management", "done", "phase-1/06.1-scene-management.md"),
    ]),
    ("Tier 2: Makes it feel like a game", [
        ("Physics", "done", "phase-1/07.1-physics-system.md"),
        ("Audio", "done", "phase-1/08.1-audio-system.md"),
        ("UI Framework", "done", "phase-1/09.1-ui-framework.md"),
    ]),
    ("Tier 3: Revisit once Tier 1-2 exist", [
        ("Rendering System (2D camera, compositing)", "progress", "phase-1/10.1-rendering-system.md"),
        ("Scripting & Events", "todo", "phase-2/11.2-scripting-and-events.md"),
    ]),
    ("Tier 4: Explicitly deferred", [
        ("Networking", "todo", "phase-2/12.2-networking.md"),
        ("Optimization", "progress", "phase-1/13.1-optimization.md"),
        ("Documentation", "progress", "phase-1/14.1-documentation.md"),
    ]),
]

BADGE_LABEL = {"done": "Done", "progress": "In progress", "todo": "Planned"}


def build_index():
    features_html = "\n".join(
        f'<div><h2>{title}</h2><p>{desc}</p></div>'
        for title, desc in FEATURES
    )

    roadmap_html = ""
    for tier_name, items in ROADMAP:
        roadmap_html += f'<h3 class="roadmap-tier">{html.escape(tier_name)}</h3><div class="roadmap-list">'
        for name, status, doc_path in items:
            badge = f'<span class="badge badge-{status}">{BADGE_LABEL[status]}</span>'
            link = f"{GITHUB_URL}/blob/main/docs/roadmap/{doc_path}"
            roadmap_html += (
                f'<div class="roadmap-item"><span class="name">{html.escape(name)}</span>'
                f'{badge}<a href="{link}" target="_blank" rel="noopener" class="tag" style="margin-left:8px;">Read the phase doc</a></div>'
            )
        roadmap_html += "</div>"

    shots = [e for e in EXAMPLES if (SITE / "assets" / "img" / "examples" / f'{e["slug"]}.png').exists()][:8]
    example_strip = "\n".join(
        f'<a href="engine/examples/{e["slug"]}.html"><figure class="shot">'
        f'<img src="assets/img/examples/{e["slug"]}.png" alt="" loading="lazy"/></figure>'
        f'<span class="name">{e["title"]}</span></a>'
        for e in shots
    )

    body = f"""
<section class="hero">
  <div class="container hero-grid">
    <div>
      <h1>Make <mark>2D games</mark> in C++.</h1>
      <p class="lede">A modular C++23 engine on SDL3 for Windows, Linux and macOS.</p>
      <div class="btn-row">
        <a class="btn btn-primary" href="engine/getting-started.html">Get started</a>
        <a class="text-link" href="engine/examples/index.html">Browse the {len(EXAMPLES)} examples</a>
      </div>
      <p id="latest-release" class="lede" hidden></p>
    </div>
    <div class="hero-demo">
      {code_card("MovingBox.cpp", "cpp", HERO_SNIPPET)}
    </div>
  </div>
</section>

<section>
  <div class="container">
    <div class="pillars">
      {features_html}
    </div>
  </div>
</section>

<section class="alt">
  <div class="container">
    <div class="section-head">
      <h2>Every example runs</h2>
      <p>Each one ships its full source and a screenshot.</p>
    </div>
    <div class="example-strip">
      {example_strip}
    </div>
    <p style="margin-top:28px;"><a class="text-link" href="engine/examples/index.html">All {len(EXAMPLES)} examples</a></p>
  </div>
</section>

<section>
  <div class="container editor-split">
    <div>
      <h2 style="margin-top:0;">Or build it in the editor</h2>
      <p class="lede">Lay out scenes, colliders and animations visually, then run them on the engine.</p>
      <div class="btn-row"><a class="btn btn-ghost" href="editor/index.html">Open the editor guide</a></div>
    </div>
    <figure class="shot"><img src="assets/img/editor/hero.png" alt="The ASGE level editor with a scene open" width="1296" height="759" loading="lazy"/></figure>
  </div>
</section>

<section id="roadmap" class="alt">
  <div class="container">
    <div class="section-head">
      <h2>Roadmap</h2>
      <p>Worked through in tiers, in priority order. Each system has its own
      <a href="{GITHUB_URL}/blob/main/docs/roadmap/README.md" target="_blank" rel="noopener">phase docs</a>.</p>
    </div>
    {roadmap_html}
  </div>
</section>
"""
    return render_page(page=PAGE, body=body, raw=True, extra_scripts='<script src="assets/js/releases.js"></script>')


HERO_SNIPPET = """void MovingBox::Update(float inDeltaTime)
{
    // dir is built from the WASD keys
    m_Position += dir * m_Speed * inDeltaTime;
}

void MovingBox::Render(asge::video::IRenderer &inRender)
{
    inRender.DrawRect(
        asge::math::Rect{ m_Position.x(), m_Position.y(), m_Dimension.x(), m_Dimension.y() },
        {255, 0, 0, 255},
        true
    );
}
"""


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
