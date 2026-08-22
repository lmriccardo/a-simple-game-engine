# Asset Pipeline — Phase 1

*Priority: Tier 1 — moved up ahead of Scene Management. See [README](../README.md).
Other phases: [Phase 2](../phase-2/05-asset-pipeline.md) (caching, async streaming),
[Phase 3](../phase-3/05-asset-pipeline.md) (live reload).*

## Goals
Formalize a common asset-handle concept before a third ad-hoc loader (audio) shows
up and triplicates the pattern, and before [06-Scene Management](06-scene-management.md)/
ECS need a way to reference assets by handle rather than a hardcoded path per demo.

## Current state
Two independent, hand-rolled loaders already exist and already do the "Basic tier"
job for their own asset type: `Image::Load(Path)`
([Image.hpp](../../../src/ASGE/Core/Graphics/Image.hpp)) and
`Font::Load(Path, pixelHeight)` ([Font.hpp](../../../src/ASGE/Core/Graphics/Font.hpp)),
both returning a `Result<T>`. There is no shared concept between them — each example
calls its own loader directly with a hardcoded path (see `LoadTexture`/`LoadFont`
helpers in `texture_demo`/`text_demo`). Nothing resembling metadata, an asset
database, or async loading exists.

## Design notes
- **Unify the existing pattern, don't replace it.** `Image::Load`/`Font::Load`
  already work and are tested; Phase 1 is about extracting the common shape (a
  `filesystem::Path` in, a `Result<T>` out) into something `AudioClip::Load` (once
  [08-Audio](08-audio-system.md) exists) can follow too, not rewriting either loader.
- **Asset references, not an asset database.** The concrete need right now is: let
  ECS components ([03-ECS](03-entity-component-system.md)'s `Sprite`/`AudioSource`)
  and future scenes ([06-Scene Management](06-scene-management.md)) hold a handle to
  an asset instead of a hardcoded path baked into example code. A full metadata
  system / compression / searchable asset database is scope creep from the original
  generic-engine template — nothing here has enough assets yet to need it.
- **`filesystem::Path`/`FileIO` already exist** ([01-Core Engine](../01-core-engine.md))
  as the underlying file-access layer every loader already builds on — Phase 1 adds a
  handle/reference concept on top, not a new I/O layer underneath.

## Tasks
- [ ] A common asset-handle type (e.g. `Asset<T>` or similar) wrapping "loaded value
      + the path it came from", built from the shape `Image::Load`/`Font::Load`
      already share
- [ ] Asset references usable from ECS components instead of a raw path per demo
- [ ] Dropped from Phase 1: metadata system, compression, asset database — no
      concrete need for any of them yet; revisit only if a real requirement appears

## Deliverables
- A shared asset-handle concept that `Image::Load`/`Font::Load` (and eventually
  audio) all produce
- At least one ECS component or example referencing an asset by handle instead of a
  hardcoded path

## Explicitly out of scope for Phase 1
Caching and async streaming ([Phase 2](../phase-2/05-asset-pipeline.md)), live
reload ([Phase 3](../phase-3/05-asset-pipeline.md)), and a full
metadata/compression/database system — no concrete requirement for any of it yet.
