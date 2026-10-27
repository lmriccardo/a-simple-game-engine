"""Releases page: one merged Engine + Editor timeline filled by assets/js/releases.js."""
from _shared import GITHUB_URL, Page, register, render_page

PAGE = register(Page(
    "releases.html", "Releases", "releases", "",
    "Every ASGE engine and editor release, with installers for Windows, Linux and macOS.",
))

BODY = f"""
<h1>Releases</h1>
<p class="lede">Packaged builds for Windows, Linux and macOS, generated automatically on every tagged release.
A tag like <code>v0.8.8e1.5</code> is engine 0.8.8 shipped with editor 1.5; a plain <code>v0.8.4</code> is an
engine-only release. Pulled live from the GitHub Releases API.</p>
<div class="btn-row" role="group" aria-label="Filter releases">
  <button class="btn btn-ghost" data-filter="all" aria-pressed="true">All</button>
  <button class="btn btn-ghost" data-filter="engine" aria-pressed="false">Engine</button>
  <button class="btn btn-ghost" data-filter="editor" aria-pressed="false">Editor</button>
</div>
<p id="releases-fallback" class="lede">Loading releases... if they don't appear, browse them directly on
<a href="{GITHUB_URL}/releases" target="_blank" rel="noopener">GitHub</a> or read the
<a href="{GITHUB_URL}/blob/main/CHANGELOG.md" target="_blank" rel="noopener">CHANGELOG</a>.</p>
<div id="releases-list"></div>
"""

SCRIPTS = (
    '<script src="https://cdnjs.cloudflare.com/ajax/libs/marked/13.0.2/marked.min.js"></script>\n'
    '<script src="https://cdnjs.cloudflare.com/ajax/libs/dompurify/3.1.6/purify.min.js"></script>\n'
    '<script src="assets/js/releases.js"></script>'
)


def build() -> dict[str, str]:
    return {"releases.html": render_page(page=PAGE, body=BODY, extra_scripts=SCRIPTS)}
