#!/usr/bin/env python3
"""Page shell shared by generate_site.py and generate_api.py.

Both scripts render pages into the same static docsite/site/ tree with the
same nav/footer/theme chrome; this module is the one place that chrome is
defined so the two generators can't drift apart.
"""
import html
import re
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SITE = ROOT / "site"
REPO = "lmriccardo/a-simple-game-engine"
GITHUB_URL = f"https://github.com/{REPO}"

@dataclass(frozen=True)
class Page:
    """One page of the site. `path` is site-relative; `group` is the sidebar heading."""
    path: str
    title: str
    section: str  # overview | engine | editor | releases | api
    group: str
    summary: str

    @property
    def depth(self) -> int:
        return self.path.count("/")


# Every non-API page registers itself here; nav, sidebars, search and the
# link checker are all derived from it.
SITE_MAP: list[Page] = []

# (section key, label, landing page). API pages highlight "engine".
SECTIONS = [
    ("overview", "Overview", "index.html"),
    ("engine", "Engine", "engine/getting-started.html"),
    ("editor", "Editor", "editor/index.html"),
    ("releases", "Releases", "releases.html"),
]


def register(page: Page) -> Page:
    SITE_MAP[:] = [p for p in SITE_MAP if p.path != page.path] + [page]
    return page


def rel(depth: int, path: str) -> str:
    return ("../" * depth) + path


def render_nav(page: Page) -> str:
    active = "engine" if page.section == "api" else page.section
    cur = ' aria-current="page"'
    links = [
        f'<a href="{rel(page.depth, path)}"{cur if key == active else ""}>{label}</a>'
        for key, label, path in SECTIONS
    ]
    links.append(f'<a href="{GITHUB_URL}" target="_blank" rel="noopener">GitHub</a>')
    return "\n    ".join(links)


def render_sidebar(page: Page) -> str:
    """Pages of the current section grouped by `group`; the current one is marked."""
    groups: dict[str, list[Page]] = {}
    for p in SITE_MAP:
        if p.section == page.section and p.group:
            groups.setdefault(p.group, []).append(p)
    out = []
    for name, pages in groups.items():
        items = []
        for p in pages:
            cur = ' aria-current="page"' if p.path == page.path else ""
            items.append(f'<li><a href="{rel(page.depth, p.path)}"{cur}>{html.escape(p.title)}</a></li>')
        out.append(f'<div class="side-group"><h4>{html.escape(name)}</h4><ul>{"".join(items)}</ul></div>')
    return "\n".join(out)


def _slug(text: str) -> str:
    text = html.unescape(re.sub(r"<[^>]+>", "", text)).lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-") or "section"


_HEADING = re.compile(r"<h([23])([^>]*)>(.*?)</h\1>", re.S)


def add_heading_ids(body: str) -> str:
    """Give every h2/h3 a unique slug id (existing ids are kept)."""
    seen: set[str] = set(re.findall(r'<h[23][^>]*\sid="([^"]+)"', body))

    def fix(m):
        level, attrs, inner = m.groups()
        if ' id="' in attrs:
            return m.group(0)
        base = slug = _slug(inner)
        n = 2
        while slug in seen:
            slug, n = f"{base}-{n}", n + 1
        seen.add(slug)
        return f'<h{level}{attrs} id="{slug}">{inner}</h{level}>'

    return _HEADING.sub(fix, body)


def build_toc(body: str) -> str:
    """On-page table of contents from h2/h3 that have ids; "" if there are none."""
    items = []
    for level, attrs, inner in _HEADING.findall(body):
        m = re.search(r'id="([^"]+)"', attrs)
        if m:
            text = html.escape(html.unescape(re.sub(r"<[^>]+>", "", inner)))
            items.append(f'<li class="l{level}"><a href="#{m.group(1)}">{text}</a></li>')
    if not items:
        return ""
    return f'<nav class="toc" aria-label="On this page"><h4>On this page</h4><ul>{"".join(items)}</ul></nav>'


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


def render_page(*, page: Page, body: str, sidebar: str | None = None, raw: bool = False,
                extra_head: str = "", extra_scripts: str = "") -> str:
    """Wrap `body` in the site shell. `raw=True` skips sidebar/TOC (hero-style pages)."""
    depth = page.depth
    css = rel(depth, "assets/css/style.css")
    logo = rel(depth, "assets/img/logo.svg")
    favicon = rel(depth, "assets/img/favicon.svg")
    main_js = rel(depth, "assets/js/main.js")
    desc = html.escape(page.summary)
    title = html.escape(page.title)
    side = sidebar if sidebar is not None else render_sidebar(page)
    if raw:
        content = body
    else:
        body = add_heading_ids(body)
        toc = build_toc(body)
        cls = "layout" + ("" if side else " no-sidebar")
        parts = [f'<div class="container {cls}">']
        if side:
            parts.append(f'<aside class="sidebar" aria-label="Section navigation">{side}</aside>')
        parts.append(f'<article class="article">{body}</article>')
        if toc:
            parts.append(f'<aside class="toc-wrap">{toc}</aside>')
        parts.append("</div>")
        content = "\n".join(parts)

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{title} · ASGE</title>
<meta name="description" content="{desc}"/>
<meta property="og:title" content="{title} · ASGE"/>
<meta property="og:description" content="{desc}"/>
<link rel="icon" type="image/svg+xml" href="{favicon}"/>
<link rel="stylesheet" href="{css}"/>
{extra_head}
</head>
<body data-root="{rel(depth, '') or './'}">
<header class="site-header">
  <div class="container">
    <a class="brand" href="{rel(depth, 'index.html')}"><img src="{logo}" alt=""/> ASGE</a>
    <nav class="nav-links" aria-label="Sections">
    {render_nav(page)}
    </nav>
    <div class="nav-right">
      <button class="search-btn" data-search-open aria-label="Search the docs">{SEARCH_ICON}<span>Search</span><kbd>Ctrl K</kbd></button>
      <button class="icon-btn" data-theme-toggle title="Toggle color theme" aria-label="Toggle color theme">{THEME_ICON}</button>
      <button class="icon-btn nav-toggle" data-nav-toggle title="Menu" aria-label="Toggle navigation menu" aria-controls="drawer">{MENU_ICON}</button>
    </div>
  </div>
</header>
<div class="drawer" id="drawer">
  <nav class="drawer-sections" aria-label="Sections">
  {render_nav(page)}
  </nav>
  {side}
</div>
<main>
{content}
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
