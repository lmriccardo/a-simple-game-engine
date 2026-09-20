#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${repo_root}"

# Dear ImGui is compiled from source as part of the ASGE build (see the
# `imgui` target in the root CMakeLists.txt), so -- unlike SDL -- there's no
# build/install step here, just the source tree. Tracked on `main`, mirroring
# install-stb.sh's pattern; the .git folder is stripped so it isn't a nested
# repo inside this one.
rm -rf third-party/imgui
git clone --depth 1 https://github.com/ocornut/imgui.git third-party/imgui
rm -rf third-party/imgui/.git
