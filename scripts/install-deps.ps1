$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."

# Single entry point for CI/devs to fetch every vendored third-party
# dependency. Add new dependencies here rather than wiring them into CI
# workflows individually.
& "$RepoRoot/scripts/install-sdl-windows.ps1"
& "$RepoRoot/scripts/install-stb-image.ps1"
