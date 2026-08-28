#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${repo_root}"

# stb_image.h and friends are single public-domain headers with no build
# step, so unlike SDL there's nothing to clone+build -- just fetch each file.
# Pinned to master since stb doesn't publish release tags; swap in a commit
# SHA below if you want fully reproducible builds.
cmake -E make_directory third-party/stb

stb_headers=(
    stb_image.h
    stb_truetype.h
)

for header in "${stb_headers[@]}"; do
    curl -fsSL -o "third-party/stb/${header}" \
        "https://raw.githubusercontent.com/nothings/stb/master/${header}"
done
