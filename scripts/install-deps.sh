#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Single entry point for CI/devs to fetch every vendored third-party
# dependency. Add new dependencies here rather than wiring them into CI
# workflows individually.
bash "${repo_root}/scripts/install-sdl-linux.sh"
bash "${repo_root}/scripts/install-stb-image.sh"
