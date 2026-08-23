#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Single entry point for CI/devs to fetch every vendored third-party
# dependency. Add new dependencies here rather than wiring them into CI
# workflows individually.
case "$(uname -s)" in
    Darwin)
        bash "${repo_root}/scripts/install-sdl-mac.sh"
        ;;
    *)
        bash "${repo_root}/scripts/install-sdl-linux.sh"
        ;;
esac
bash "${repo_root}/scripts/install-stb.sh"
