#pragma once

#include <memory>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Audio/AudioDevice.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief An entity's audio clip plus its current playback state.
 *
 * m_VirtualClipPath (set directly or via scene deserialization) is resolved
 * to m_Clip by asset::AssetManager::ResolveAssets, same as Sprite/Animation
 * resolve their own paths. m_Stream is left null until systems::AudioSystem
 * first plays this source -- once assigned, it points into the owning
 * audio::AudioDevice's stream pool and stays valid for this source's
 * lifetime (see AudioDevice's doc comment); replaying it (see
 * PlayAudioSource) always reuses that same stream rather than creating a
 * new one, until DetachAudioSource releases it back to the pool. Use
 * PlayAudioSource/StopAudioSource/DetachAudioSource rather than touching
 * m_Playing/m_Stream directly, so m_Loop and m_Restart stay in sync with
 * the request.
 */
struct AudioSource
{
    using audio_clip_asset = std::shared_ptr<asset::Asset<media::AudioClip>>;
    using stream = std::shared_ptr<audio::AudioStream>;

    audio_clip_asset    m_Clip              { nullptr }; // resolved clip asset, or null until ResolveAssets runs
    stream              m_Stream            { nullptr }; // this source's slot in the AudioDevice pool, once played
    str::String         m_VirtualClipPath   {};          // VFS path resolved into m_Clip
    bool                m_Playing           { false };   // whether AudioSystem should be advancing playback
    bool                m_Loop              { false };   // whether AudioSystem restarts the clip when it runs out
    bool                m_Restart           { false };   // set by PlayAudioSource; consumed once by AudioSystem to force an immediate (re)start, independent of m_Loop or how much data is still queued
    float               m_Volume            { 1.0f };    // this source's own playback gain; applied to m_Stream via SetAudioGain whenever AudioSystem (re)starts it
};

/**
 * @brief Marks inAudioSource to (re)start playing right away, looping per
 *        inLoop once it runs out of queued audio.
 *
 * Safe to call whether inAudioSource is stopped, already playing, or still
 * mid-loop -- AudioSystem always honors the request on its next pass by
 * restarting from the beginning of the clip, reusing the existing stream
 * rather than creating a new one. inLoop only governs what happens if this
 * playthrough runs out on its own; it plays independently of it either way.
 */
void PlayAudioSource( AudioSource& inAudioSource, bool inLoop = true ) noexcept;

/** @brief Marks inAudioSource to stop; AudioSystem clears its queued audio on the next pass. */
void StopAudioSource( AudioSource& inAudioSource ) noexcept;

/**
 * @brief Releases inAudioSource's stream back to inDevice's pool and clears
 *        m_Stream, so a later PlayAudioSource creates a fresh one instead of
 *        reusing it.
 *
 * A no-op returning BoolResult::Ok() if inAudioSource has no stream yet.
 * Unlike StopAudioSource, this actually frees the underlying pool slot for
 * other sources to use -- reach for it when a source is done for good (e.g.
 * its entity is being destroyed), not just paused.
 */
BoolResult DetachAudioSource( audio::AudioDevice& inDevice, AudioSource& inAudioSource ) noexcept;

/** @brief Set the audio source gain/volume */
void SetVolume( AudioSource& inAudioSource, float inVolume ) noexcept;

/**
 * @brief Round-trips m_VirtualClipPath only — which clip to play, not the
 *        live playback state. FromToml leaves m_Clip null (resolved later
 *        by AssetManager::ResolveAssets, same as Sprite::m_Texture) and
 *        m_Stream/m_Playing/m_Loop/m_Volume at AudioSource's in-code
 *        defaults; a scene file describes what an entity plays, not
 *        whether a previous run happened to be mid-playback.
 */
template<>
struct Serializer<AudioSource>
{
    static constexpr str::StringView kTableName = "AudioSource";

    using T = AudioSource;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    scene::SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    scene::LoadContext const& inCtx ) noexcept;
};

}