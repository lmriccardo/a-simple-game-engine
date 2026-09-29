#!/usr/bin/env python3
"""Page shell shared by generate_site.py and generate_api.py.

Both scripts render pages into the same static docsite/site/ tree with the
same nav/footer/theme chrome; this module is the one place that chrome is
defined so the two generators can't drift apart.
"""
import html
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SITE = ROOT / "site"
REPO = "lmriccardo/a-simple-game-engine"
GITHUB_URL = f"https://github.com/{REPO}"

NAV_ITEMS = [
    ("overview", "Overview", "index.html"),
    ("install", "Install", "install.html"),
    ("examples", "Examples", "examples/index.html"),
    ("editor", "Editor", "editor/index.html"),
    ("api", "API Reference", "api/index.html"),
    ("releases", "Releases", "releases.html"),
]

# The pages under the navbar's "Editor" dropdown (the "editor" NAV_ITEMS entry
# above is its toggle and links to the overview). A page in this list is
# active as "editor/<key>"; the editor overview itself is just "editor".
EDITOR_PAGES = [
    ("projects-and-scenes", "Projects & Scenes", "editor/projects-and-scenes.html"),
    ("entities-and-components", "Entities & Components", "editor/entities-and-components.html"),
    ("ui-and-layout", "UI & Layout", "editor/ui-and-layout.html"),
    ("viewport-and-navigation", "Viewport & Navigation", "editor/viewport-and-navigation.html"),
    ("assets-and-vfs", "Assets & VFS", "editor/assets-and-vfs.html"),
    ("advanced-editing", "Advanced Editing", "editor/advanced-editing.html"),
    ("releases", "Editor Releases", "editor/releases.html"),
]

CHEVRON_ICON = (
    '<svg class="chev" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" '
    'stroke-width="3" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
    '<path d="m6 9 6 6 6-6"/></svg>'
)


def rel(depth: int, path: str) -> str:
    return ("../" * depth) + path


def render_editor_menu(active: str, depth: int) -> str:
    items = []
    for key, label, path in EDITOR_PAGES:
        cls = ' class="active"' if active == f"editor/{key}" else ""
        items.append(f'<a href="{rel(depth, path)}"{cls} role="menuitem">{html.escape(label)}</a>')
    return "\n      ".join(items)


def render_nav(active: str, depth: int) -> str:
    links = []
    for key, label, path in NAV_ITEMS:
        if key == "editor":
            cls = ' class="active"' if active == "editor" or active.startswith("editor/") else ""
            links.append(
                '<div class="nav-dropdown">\n'
                f'    <a href="{rel(depth, path)}"{cls} aria-haspopup="true">{label}{CHEVRON_ICON}</a>\n'
                f'    <div class="nav-menu" role="menu">\n      {render_editor_menu(active, depth)}\n    </div>\n'
                '    </div>'
            )
            continue
        cls = ' class="active"' if key == active else ""
        links.append(f'<a href="{rel(depth, path)}"{cls}>{label}</a>')
    links.append(f'<a href="{GITHUB_URL}" target="_blank" rel="noopener">GitHub</a>')
    return "\n    ".join(links)


_NAV_BLOCK = re.compile(r'<nav class="nav-links">.*?</nav>', re.S)


def refresh_editor_nav():
    """Re-render the navbar of the hand-written pages under site/editor/.

    Those pages aren't produced by a generator, so without this their copy of
    the nav (now including the Editor dropdown) would drift from render_nav()
    the moment NAV_ITEMS/EDITOR_PAGES change. The rest of the page is left
    exactly as written.
    """
    for path in sorted((SITE / "editor").glob("*.html")):
        active = "editor" if path.stem == "index" else f"editor/{path.stem}"
        text = path.read_text(encoding="utf-8")
        block = f'<nav class="nav-links">\n    {render_nav(active, 1)}\n    </nav>'
        updated, count = _NAV_BLOCK.subn(lambda _m: block, text, count=1)
        if count and updated != text:
            path.write_text(updated, encoding="utf-8", newline="\n")
            print(f"refreshed nav in {path.relative_to(ROOT)}")


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

SEARCH_ICON = (
    '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" '
    'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
    '<circle cx="11" cy="11" r="7"></circle><path d="m21 21-4.3-4.3"></path></svg>'
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
