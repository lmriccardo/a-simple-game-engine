$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."

Set-Location $RepoRoot

# Dear ImGui is compiled from source as part of the ASGE build (see the
# `imgui` target in the root CMakeLists.txt), so -- unlike SDL -- there's no
# build/install step here, just the source tree. Tracked on `main`, mirroring
# install-stb.ps1's pattern; the .git folder is stripped so it isn't a nested
# repo inside this one.
cmake -E remove_directory third-party/imgui
git clone --depth 1 https://github.com/ocornut/imgui.git third-party/imgui
cmake -E remove_directory third-party/imgui/.git
