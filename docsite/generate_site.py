#!/usr/bin/env python3
"""Renders docsite/site/**/*.html from the content modules in docsite/content/.

This is authoring tooling, not part of the CI/deploy pipeline: the output is
committed straight into docsite/site/ like any other static asset, so GitHub
Pages never has to run Python. Re-run it (`python docsite/generate_site.py`)
after editing a content module or the shared layout in _shared.py. The API
reference (site/engine/api/) is built separately by generate_api.py in CI.
"""
import json

from _shared import SITE, build_search_index, write
from content import editor, engine, overview, releases, theory  # importing registers each module's pages

MODULES = [overview, engine, theory, editor, releases]


def main():
    rendered = {}
    for module in MODULES:
        rendered.update(module.build())
    for path, html_text in rendered.items():
        write(SITE / path, html_text)
    write(SITE / "assets" / "search-index.json", json.dumps(build_search_index(rendered)))


if __name__ == "__main__":
    main()
