"""Engine section: Getting Started, API landing entry and the examples."""
import html

from _shared import GITHUB_URL, ROOT, SITE, Page, code_card, register, render_page
from content.examples_data import EXAMPLES

GETTING_STARTED = register(Page(
    "engine/getting-started.html", "Getting Started", "engine", "Start",
    "Build ASGE from source, or consume it from your own C++ project via add_subdirectory, FetchContent, or an installed CMake package.",
))
register(Page(
    "engine/api/index.html", "API Reference", "engine", "Reference",
    "Classes, structs and namespaces of the ASGE engine, generated from the source.",
))
EXAMPLES_INDEX = register(Page(
    "engine/examples/index.html", "All examples", "engine", "Examples",
    f"{len(EXAMPLES)} runnable ASGE examples covering rendering, ECS, input, physics, audio, scenes and more.",
))
EXAMPLE_PAGES = {
    e["slug"]: register(Page(f"engine/examples/{e['slug']}.html", e["title"], "engine", "Examples", e["tagline"]))
    for e in EXAMPLES
}

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
    return render_page(page=GETTING_STARTED, body=body)


def example_card(e: dict) -> str:
    img_path = f"assets/img/examples/{e['slug']}.png"
    has_img = (SITE / img_path).exists()
    if has_img:
        thumb = f'<img src="../../{img_path}" alt="{html.escape(e["title"])} screenshot" loading="lazy"/>'
    elif e["kind"] == "console":
        thumb = '<div class="noshot">console example<br/>(no window)</div>'
    else:
        # A window-kind example without a captured screenshot yet (e.g. just
        # added) -- distinct from "console example" so the grid doesn't claim
        # it has no window at all.
        thumb = '<div class="noshot">screenshot pending</div>'
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
    return render_page(page=EXAMPLES_INDEX, body=body)


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
            f'<img src="../../{img_path}" alt="{html.escape(e["title"])} screenshot"/>'
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
      <button class="btn btn-primary" id="play-btn" data-target="../../playable/{slug}/index.html">Play {html.escape(e['title'])} in your browser</button>
      <div id="play-frame-wrap" hidden style="margin-top:16px;" class="example-media">
        <iframe id="play-frame" title="{html.escape(e['title'])} (playable build)" allow="autoplay"></iframe>
      </div>
    </div>
    <script>
      (function(){{
        var url = "../../playable/{slug}/index.html";
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
    <p class="lede">See <a href="../getting-started.html">Installation</a> for the full build setup.</p>
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
    return render_page(page=EXAMPLE_PAGES[slug], body=body)


def build() -> dict[str, str]:
    pages = {
        "engine/getting-started.html": build_install(),
        "engine/examples/index.html": build_examples_index(),
    }
    for e in EXAMPLES:
        pages[f"engine/examples/{e['slug']}.html"] = build_example_detail(e)
    return pages
