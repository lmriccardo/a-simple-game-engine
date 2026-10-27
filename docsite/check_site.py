#!/usr/bin/env python3
"""Checks the generated docsite: broken links, one <h1> per page, unique ids.

Usage: python docsite/check_site.py [--site DIR] [--allow-missing PREFIX ...] [--selftest]
"""
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent


class _Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids, self.h1, self.dupes = [], set(), 0, []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "h1":
            self.h1 += 1
        if a.get("id"):
            if a["id"] in self.ids:
                self.dupes.append(a["id"])
            self.ids.add(a["id"])
        for key in ("href", "src"):
            if a.get(key):
                self.links.append(a[key])


def _parse(path: Path) -> _Page:
    page = _Page()
    page.feed(path.read_text(encoding="utf-8"))
    return page


def check(site: Path, allow_missing: tuple[str, ...] = ()) -> list[str]:
    """Return a list of problems found under `site` (empty means OK)."""
    problems, pages = [], {p: _parse(p) for p in sorted(site.rglob("*.html"))}
    for path, page in pages.items():
        name = path.relative_to(site).as_posix()
        if page.h1 != 1:
            problems.append(f"{name}: expected exactly one h1, found {page.h1}")
        problems += [f"{name}: duplicate id {i!r}" for i in page.dupes]
        for link in page.links:
            u = urlsplit(link)
            if u.scheme or u.netloc:
                continue
            target = path if not u.path else (path.parent / unquote(u.path)).resolve()
            rel = target.relative_to(site.resolve()).as_posix() if target.is_relative_to(site.resolve()) else u.path
            if any(rel.startswith(a) for a in allow_missing):
                continue
            if target.is_dir():
                target = target / "index.html"
            if not target.exists():
                problems.append(f"{name}: broken link {link}")
            elif u.fragment and target.suffix == ".html":
                tp = pages.get(target) or pages.get(target.resolve())
                if tp is not None and u.fragment not in tp.ids:
                    problems.append(f"{name}: dangling #{u.fragment} in {link}")
    return problems


def selftest():
    with tempfile.TemporaryDirectory() as tmp:
        site = Path(tmp)
        (site / "good.html").write_text('<h1>A</h1><a href="other.html#x">o</a><a href="gone/a.html">g</a><a href="https://x.y">e</a>')
        (site / "other.html").write_text('<h1>B</h1><h2 id="x">x</h2>')
        assert check(site, ("gone/",)) == [], check(site, ("gone/",))
        (site / "broken.html").write_text('<h1>C</h1><a href="nope.html">n</a>')
        assert any("nope.html" in p for p in check(site)), "broken link not reported"
        (site / "broken.html").write_text('<p>no heading</p>')
        assert any("h1" in p for p in check(site)), "missing h1 not reported"
        (site / "broken.html").write_text('<h1>C</h1><h2 id="d"></h2><h2 id="d"></h2>')
        assert any("duplicate id" in p for p in check(site)), "duplicate id not reported"
        (site / "broken.html").write_text('<h1>C</h1><a href="other.html#missing">m</a>')
        assert any("#missing" in p for p in check(site)), "dangling fragment not reported"
    print("selftest ok")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
        sys.exit(0)
    argv = sys.argv[1:]
    allow = tuple(argv[i + 1] for i, a in enumerate(argv) if a == "--allow-missing")
    site = Path(argv[argv.index("--site") + 1]) if "--site" in argv else ROOT / "site"
    found = check(site, allow)
    print(chr(10).join(found) or "site ok")
    sys.exit(1 if found else 0)
