# Audio System — Phase 1

*Priority: Tier 2 — low coupling to everything else. See [README](../README.md).
Other phases: [Phase 2](../phase-2/08-audio-system.md) (spatial audio, music
streaming), [Phase 3](../phase-3/08-audio-system.md) (DSP effects, reverb zones).*

## Goals
Play a sound, control its volume — the minimum that makes anything feel alive.

## Current state
No audio code exists anywhere in the engine yet — no backend, no loader, no
`AudioSource` implementation. [03-ECS Phase 1](../phase-1/03-entity-component-system.md)
already stubs an `AudioSource` component specifically for this system to fill in.
SDL3's audio subsystem (`SDL_audio.h`) is already vendored under `third-party/SDL`,
alongside the video/input subsystems this engine already uses.

## Design notes
- **Wants [05-Asset Pipeline Phase 1](../phase-1/05-asset-pipeline.md)'s
  asset-handle pattern before it starts**, per that doc's own goal: land the
  unified loader shape (`Image::Load`/`Font::Load`'s `Path` → `Result<T>` pattern)
  before a third ad-hoc loader (`AudioClip::Load` for WAV/OGG) triplicates it
  instead of following it.
- **SDL_audio as the backend**, matching how video ([SDLRenderer](../02-rendering-primitives.md))
  and input ([SDL_TranslationUnit](../phase-1/04-input-system.md)) already build on
  SDL rather than a separate audio library — one fewer third-party dependency.
- **A system, not a singleton `AudioManager` god-object.** Following
  [03-ECS](03-entity-component-system.md)'s convention: an `AudioSystem` operating
  on a view of `AudioSource` components, playing/stopping clips based on component
  state, not a global manager games call into directly.

## Tasks
- [ ] Audio backend initialization via `SDL_audio` (mirrors how
      [01-Core Engine](../01-core-engine.md)'s `VideoSystem` initializes the SDL
      video backend)
- [ ] WAV/OGG loading, following [05-Asset Pipeline](05-asset-pipeline.md)'s
      asset-handle shape
- [ ] `AudioSource` component: clip handle, volume, playing state
- [ ] `AudioSystem`: plays/stops clips based on `AudioSource` state, same shape as
      `MovementSystem`/`RenderSystem`
- [ ] Master/per-source volume control

## Deliverables
- At least one example playing a sound with adjustable volume
- `AudioSource` wired into the ECS the same way `Sprite` already is

## Explicitly out of scope for Phase 1
3D/spatial audio, music streaming ([Phase 2](../phase-2/08-audio-system.md)); DSP
effects, reverb zones ([Phase 3](../phase-3/08-audio-system.md)); a full mixer graph
— Phase 1 is master + per-source volume only.
