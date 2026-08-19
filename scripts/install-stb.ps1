$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."

Set-Location $RepoRoot

# stb_image.h and friends are single public-domain headers with no build
# step, so unlike SDL there's nothing to clone+build -- just fetch each file.
# Pinned to master since stb doesn't publish release tags; swap in a commit
# SHA below if you want fully reproducible builds.
New-Item -ItemType Directory -Force -Path third-party/stb | Out-Null

$StbHeaders = @(
    "stb_image.h",
    "stb_truetype.h"
)

foreach ($Header in $StbHeaders) {
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/$Header" `
        -OutFile "third-party/stb/$Header"
}
