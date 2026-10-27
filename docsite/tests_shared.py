#!/usr/bin/env python3
"""Plain-assert tests for the docsite generators. Run: python docsite/tests_shared.py"""
import _shared as s


def _reset(*pages):
    s.SITE_MAP.clear()
    for p in pages:
        s.register(p)


def test_add_heading_ids_dedupes():
    out = s.add_heading_ids("<h2>Setup</h2><h2>Setup</h2><h3>A &amp; B</h3>")
    assert 'id="setup"' in out and 'id="setup-2"' in out and 'id="a-b"' in out, out


def test_toc_empty_without_headings():
    assert s.build_toc("<p>x</p>") == ""


def test_toc_lists_headings():
    toc = s.build_toc(s.add_heading_ids("<h2>One</h2><h3>Two</h3>"))
    assert 'href="#one"' in toc and 'href="#two"' in toc, toc


def test_sidebar_marks_current_and_scopes_section():
    a = s.Page("engine/theory/ecs.html", "ECS", "engine", "Theory", "x")
    b = s.Page("engine/theory/loop.html", "Loop", "engine", "Theory", "x")
    c = s.Page("editor/index.html", "Editor", "editor", "Guide", "x")
    _reset(a, b, c)
    html = s.render_sidebar(a)
    assert html.count('aria-current="page"') == 1 and "Loop" in html and "Editor" not in html, html


def test_render_page_depth_and_toc():
    p = s.Page("engine/theory/ecs.html", "ECS", "engine", "Theory", "ECS page")
    _reset(p)
    out = s.render_page(page=p, body="<h1>ECS</h1><h2>Why</h2>")
    assert 'href="../../assets/css/style.css"' in out and 'class="toc"' in out, out[:600]


def test_render_page_no_toc_when_no_headings():
    p = s.Page("releases.html", "Releases", "releases", "", "x")
    _reset(p)
    assert 'class="toc"' not in s.render_page(page=p, body="<h1>R</h1>")


def test_drawer_has_section_links_for_raw_pages():
    p = s.Page("index.html", "Home", "overview", "", "x")
    _reset(p)
    out = s.render_page(page=p, body="<h1>Home</h1>", raw=True)
    assert 'id="drawer"' in out and 'href="engine/getting-started.html"' in out, out[:900]


def test_sidebar_group_order():
    ex = s.Page("engine/examples/a.html", "A", "engine", "Examples", "x")
    st = s.Page("engine/getting-started.html", "GS", "engine", "Start", "x")
    th = s.Page("engine/theory/ecs.html", "ECS", "engine", "Theory", "x")
    _reset(ex, th, st)
    html = s.render_sidebar(st)
    assert html.index("Start") < html.index("Theory") < html.index("Examples"), html


def test_api_fixture_renders_through_shared_layout():
    import json
    import tempfile
    from pathlib import Path

    import generate_api as g

    with tempfile.TemporaryDirectory() as tmp:
        g.XML_DIR = Path(__file__).resolve().parent / "tests" / "xml_fixture"
        g.OUT = Path(tmp)
        g.main()
        page = (g.OUT / "classasge_1_1Foo.html").read_text(encoding="utf-8")
        assert "asge::Foo" in page and 'class="sidebar"' in page, page[:500]
        assert 'href="../../assets/css/style.css"' in page
        index = json.loads((g.OUT / "search-index.json").read_text(encoding="utf-8"))
        assert any(e["url"] == "engine/api/classasge_1_1Foo.html" and e["section"] == "API" for e in index), index


if __name__ == "__main__":
    for name, fn in sorted(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
