#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# On macOS SDL3 is installed via Homebrew rather than built from source.
# The stb single-header libraries are fetched separately by install-stb.sh.
command -v brew >/dev/null 2>&1 || {
    echo "brew not found on PATH. Install Homebrew (https://brew.sh) and re-run." >&2
    exit 1
}

brew install sdl3
