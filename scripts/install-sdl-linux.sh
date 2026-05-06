#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential git libasound2-dev libdrm-dev \
    libegl1-mesa-dev libgbm-dev libgl1-mesa-dev \
    libibus-1.0-dev libpulse-dev libudev-dev libwayland-dev \
    libx11-dev libxcursor-dev libxext-dev libxfixes-dev \
    libxi-dev libxinerama-dev libxkbcommon-dev libxrandr-dev \
    libxss-dev ninja-build pkg-config wayland-protocols

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${repo_root}"

cmake -E remove_directory third-party/SDL
git clone --depth 1 --branch release-3.2.x https://github.com/libsdl-org/SDL.git third-party/SDL-src

cmake -S third-party/SDL-src -B build-sdl -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${repo_root}/third-party/SDL" \
    -DSDL_TEST_LIBRARY=OFF

cmake --build build-sdl --config Release
cmake --install build-sdl --config Release

cmake -E remove_directory third-party/SDL-src
cmake -E remove_directory build-sdl
