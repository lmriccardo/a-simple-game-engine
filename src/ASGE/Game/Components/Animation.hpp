#pragma once

#include <functional>
#include <map>
#include <memory>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Frame-by-frame playback state for a spritesheet — see
 *        systems::AnimationSystem, which steps m_CurrentFrame and writes
 *        the current frame of m_Clip's FrameTable into the owning entity's
 *        Sprite::m_SourceRect each frame.
 *
 * The frame list itself lives in m_Clip, a shared, VFS-loaded
 * `asset::FrameTable` — not embedded here — so multiple entities playing the
 * same clip share one loaded frame list instead of each holding a copy.
 * m_ClipPath is what actually round-trips through TOML (see Serializer<
 * Animation> below); m_Clip stays null until asset::AssetManager::
 * ResolveAssets resolves it (the same deferred-load step Sprite::m_Texture
 * goes through), so AnimationSystem has nothing to advance for a freshly
 * loaded/spawned entity until that's run at least once.
 *
 * m_ResolvedClipPath (also runtime-only) is what m_Clip was actually last
 * resolved from — Resolver<Animation> compares it against m_ClipPath to
 * notice a change (repointing to a new clip) or a clear (path emptied out,
 * m_Clip released back to null), rather than resolving once and never again.
 *
 * Ownership: while m_Clip is resolved, this Animation owns the Sprite's
 * m_SourceRect -- AnimationSystem overwrites it every frame, so a manual crop
 * on the same Sprite does not survive, and removing the Animation leaves the
 * last frame's rect behind rather than restoring the earlier crop.
 */
struct Animation
{
    using frame_table = std::shared_ptr<asset::Asset<asset::FrameTable>>;

    str::String m_ClipPath{}; // Virtual path of the clip's FrameTable TOML meta-file
    std::map<str::String, str::String, std::less<>> m_Clips{}; // Serialized. Named clips (idle/run/...) -> clip path, switched to by PlayClip
    frame_table m_Clip{};     // Resolved FrameTable asset -- null until AssetManager::ResolveAssets runs
    str::String m_ResolvedClipPath{}; // Runtime-only: the path m_Clip was actually last resolved from

    float       m_FrameDuration{0.1f}; // Serialized. Seconds each frame is shown before advancing, unless the clip's FrameTable gives that frame its own duration
    float       m_PlaybackSpeed{1.0f}; // Serialized. Multiplier on playback rate (2 = twice as fast); non-positive stops advancing
    std::size_t m_CurrentFrame{0};     // Not serialized -- index into m_Clip's FrameTable::m_Frames currently written to Sprite::m_SourceRect
    float       m_ElapsedTime{0.0f};   // Not serialized -- seconds accumulated toward the next frame advance
    bool        m_Loop{true};          // Serialized. Wrap to frame 0 at the end instead of stopping there
    bool        m_StartPlaying{true};  // Serialized. Initial m_Playing on load -- a scene can start this Animation paused
    bool        m_Playing{true};       // Not serialized -- whether AnimationSystem advances this Animation at all (live state, seeded from m_StartPlaying on load)
};

/**
 * @brief Resumes/starts playback from the current frame — does not reset
 *        m_CurrentFrame or m_ElapsedTime, so calling this on an already
 *        in-progress Animation continues rather than restarts it.
 */
void PlayAnimation( Animation& inAnim, bool inLoop = true ) noexcept;

/**
 * @brief Switches to the clip registered as inName in inAnim.m_Clips: points m_ClipPath at it and
 *        restarts playback from frame 0, looping or not per inLoop. Does nothing if that clip is
 *        already playing, so calling it every frame is safe. Returns false for an unknown name.
 */
bool PlayClip( Animation& inAnim, str::StringView inName, bool inLoop = true ) noexcept;

/** @brief Pauses playback in place; AnimationSystem stops advancing this Animation until PlayAnimation is called again. */
void StopAnimation( Animation& inAnim ) noexcept;

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_ClipPath, m_Clips, m_FrameDuration, m_PlaybackSpeed, m_Loop and
 *        m_StartPlaying — which clip to play, the named clips it can switch
 *        to, how fast, whether it loops and whether it starts playing, not
 *        the live playback progress. FromToml seeds m_Playing
 *        from m_StartPlaying, leaves m_Clip null and m_ResolvedClipPath empty
 *        (both resolved later by AssetManager::ResolveAssets, same as
 *        Sprite::m_Texture) and m_CurrentFrame/m_ElapsedTime at zero; a
 *        scene file describes what an entity's animation is, not where a
 *        previous run happened to leave it. Missing m_Loop/m_StartPlaying
 *        keys default to true, m_PlaybackSpeed to 1, so older scene files load as before.
 */
template<>
struct Serializer<components::Animation>
{
    static constexpr str::StringView kTableName = "Animation";

    using T = components::Animation;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
