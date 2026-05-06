$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."

Set-Location $RepoRoot

cmake -E remove_directory third-party/SDL
git clone --depth 1 --branch release-3.2.x https://github.com/libsdl-org/SDL.git third-party/SDL-src

cmake -S third-party/SDL-src -B build-sdl `
    -DCMAKE_INSTALL_PREFIX="$RepoRoot/third-party/SDL" `
    -DSDL_INSTALL_CMAKEDIR=cmake `
    -DSDL_TEST_LIBRARY=OFF

cmake --build build-sdl --config Release
cmake --install build-sdl --config Release

cmake -E remove_directory third-party/SDL-src
cmake -E remove_directory build-sdl
