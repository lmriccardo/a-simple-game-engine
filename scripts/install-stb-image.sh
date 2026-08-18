#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${repo_root}"

# stb_image.h is a single public-domain header with no build step, so unlike
# SDL there's nothing to clone+build -- just fetch the file. Pinned to master
# since stb doesn't publish release tags; swap in a commit SHA below if you
# want fully reproducible builds.
cmake -E make_directory third-party/stb
curl -fsSL -o third-party/stb/stb_image.h \
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
