$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."

Set-Location $RepoRoot

# stb_image.h is a single public-domain header with no build step, so unlike
# SDL there's nothing to clone+build -- just fetch the file. Pinned to master
# since stb doesn't publish release tags; swap in a commit SHA below if you
# want fully reproducible builds.
New-Item -ItemType Directory -Force -Path third-party/stb | Out-Null
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" `
    -OutFile "third-party/stb/stb_image.h"
