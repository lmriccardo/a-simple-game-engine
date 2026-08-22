# Asset Pipeline — Phase 3

*Priority: Later — a dev-experience nicety, not a shipping feature.
See [README](../README.md). Other phases:
[Phase 1](../phase-1/05-asset-pipeline.md) (asset-handle concept),
[Phase 2](../phase-2/05-asset-pipeline.md) (caching, async streaming).*

## Goals
Reload an asset in a running game the moment its source file changes on disk —
edit a texture or a TOML config, see it update without restarting.

## Current state
The infrastructure this needs already exists, built for a different purpose:
[01-Core Engine](../01-core-engine.md)'s configuration hot-reload already wires
`Core/Filesystem/FileWatcher.hpp` to `Core/Patterns/Signal.hpp` to detect a file
change and notify subscribers. Phase 3 is largely reusing that same
`FileWatcher`+`Signal` pattern for asset paths instead of config paths — not
building new file-watching infrastructure.

## Design notes
- **Reuse `FileWatcher`, don't reimplement it.** The config system's hot-reload
  already proves the watch → notify → reparse loop works; asset reload is the same
  shape with the cache from [Phase 2](../phase-2/05-asset-pipeline.md) as the thing
  being invalidated instead of a config value.

## Tasks
- [ ] Wire `FileWatcher` to invalidate/reload cache entries (from
      [Phase 2](../phase-2/05-asset-pipeline.md)) when their source file changes
- [ ] Propagate the reload to whatever's holding the handle (e.g. re-upload a
      changed texture to the GPU) — the part genuinely specific to each asset type

## Deliverables
- Editing an asset file on disk updates it in a running example without a restart,
  for at least one asset type (texture is the natural first candidate)

## Explicitly out of scope
Nothing beyond this — it's the last phase for this system.
