#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief What a non-looping clip does to the Sprite's crop once it finishes.
 *
 * RestoreSprite hands the Sprite its own crop back (the default, right for a
 * walking/picking character). HoldLastFrame leaves the clip's last frame on
 * the Sprite until another clip starts or m_ClipPath is cleared (right for
 * an object like a lifting bar that must stay open). Looping clips never
 * finish, so this has no effect on them.
 */
enum class FinishBehavior
{
    RestoreSprite, // Show the Sprite's own crop again after the clip ends
    HoldLastFrame  // Keep the clip's last frame on the Sprite after it ends
};

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
 * Ownership: this Animation owns the Sprite's m_SourceRect only while it is
 * playing. AnimationSystem saves the Sprite's own crop into m_RestRect when
 * playback starts, overwrites m_SourceRect every frame, and puts m_RestRect
 * back once m_Playing goes false (one system run later, so a finished
 * non-looping clip's last frame is shown for that run) or m_ClipPath is
 * cleared — except that a clip finishing under m_FinishBehavior ==
 * HoldLastFrame keeps ownership (m_HoldingLastFrame) and its last frame
 * stays on the Sprite until the next clip starts or m_ClipPath is cleared.
 * m_RestRect is only saved when ownership is first taken, so a held frame
 * is never mistaken for the Sprite's own crop. Removing the Animation mid-playback is the caller's job: the
 * Sprite keeps the last frame's rect unless m_RestRect is written back first
 * (the level editor's Remove handler does).
 */
struct Animation
{
    using frame_table = std::shared_ptr<asset::Asset<asset::FrameTable>>;

    str::String m_ClipPath{}; // Virtual path of the clip's FrameTable TOML meta-file
    std::map<str::String, str::String, std::less<>> m_Clips{}; // Serialized. Named clips (idle/run/...) -> clip path, switched to by PlayClip
    std::map<str::String, float, std::less<>> m_ClipFrameDurations{}; // Serialized. Optional per-clip frame duration (seconds) keyed by a m_Clips name; see EffectiveFrameDuration
    frame_table m_Clip{};     // Resolved FrameTable asset -- null until AssetManager::ResolveAssets runs
    str::String m_ResolvedClipPath{}; // Runtime-only: the path m_Clip was actually last resolved from

    float       m_FrameDuration{0.1f}; // Serialized. Seconds each frame is shown before advancing, unless the clip's FrameTable gives that frame its own duration
    float       m_PlaybackSpeed{1.0f}; // Serialized. Multiplier on playback rate (2 = twice as fast); non-positive stops advancing
    std::size_t m_CurrentFrame{0};     // Not serialized -- index into m_Clip's FrameTable::m_Frames currently written to Sprite::m_SourceRect
    float       m_ElapsedTime{0.0f};   // Not serialized -- seconds accumulated toward the next frame advance
    bool        m_Loop{true};          // Serialized. Wrap to frame 0 at the end instead of stopping there
    FinishBehavior m_FinishBehavior{FinishBehavior::RestoreSprite}; // Serialized. What a finished non-looping clip leaves on the Sprite
    bool        m_StartPlaying{true};  // Serialized. Initial m_Playing on load -- a scene can start this Animation paused
    bool        m_Playing{true};       // Not serialized -- whether AnimationSystem advances this Animation at all (live state, seeded from m_StartPlaying on load)
    std::optional<math::Rect> m_RestRect{}; // Not serialized -- the Sprite's own m_SourceRect, saved while this Animation owns it
    bool        m_OwnsSourceRect{false};    // Not serialized -- true between playback starting and the Sprite's crop being restored
    bool        m_HoldingLastFrame{false};  // Not serialized -- true while a finished HoldLastFrame clip's last frame is kept on the Sprite
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

/**
 * @brief The fallback seconds-per-frame for inAnim's current clip: the m_ClipFrameDurations override of a
 *        named clip whose path is m_ClipPath if it has a positive one, else m_FrameDuration. A per-frame
 *        entry in the clip's FrameTable still takes precedence over this.
 */
[[nodiscard]] float EffectiveFrameDuration( Animation const& inAnim ) noexcept;

/** @brief Pauses playback in place; AnimationSystem stops advancing this Animation until PlayAnimation is called again. */
void StopAnimation( Animation& inAnim ) noexcept;

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_ClipPath, m_Clips, m_ClipFrameDurations, m_FrameDuration, m_PlaybackSpeed, m_Loop,
 *        m_FinishBehavior and m_StartPlaying — which clip to play, the named clips it can switch
 *        to, how fast, whether it loops, what a finished clip leaves behind and whether it starts playing, not
 *        the live playback progress. FromToml seeds m_Playing
 *        from m_StartPlaying, leaves m_Clip null and m_ResolvedClipPath empty
 *        (both resolved later by AssetManager::ResolveAssets, same as
 *        Sprite::m_Texture) and m_CurrentFrame/m_ElapsedTime at zero; a
 *        scene file describes what an entity's animation is, not where a
 *        previous run happened to leave it. Missing m_Loop/m_StartPlaying
 *        keys default to true, m_PlaybackSpeed to 1 and a missing or unknown m_FinishBehavior to
 *        RestoreSprite, so older scene files load as before.
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
