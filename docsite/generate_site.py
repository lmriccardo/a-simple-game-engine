#!/usr/bin/env python3
"""Renders docsite/site/**/*.html from the content modules in docsite/content/.

This is authoring tooling, not part of the CI/deploy pipeline: the output is
committed straight into docsite/site/ like any other static asset, so GitHub
Pages never has to run Python. Re-run it (`python docsite/generate_site.py`)
after editing a content module or the shared layout in _shared.py. The API
reference (site/engine/api/) is built separately by generate_api.py in CI.
"""
from _shared import SITE, write
from content import editor, engine, overview, theory  # importing registers each module's pages

MODULES = [overview, engine, theory, editor]


def main():
    for module in MODULES:
        for path, html_text in module.build().items():
            write(SITE / path, html_text)


if __name__ == "__main__":
    main()
