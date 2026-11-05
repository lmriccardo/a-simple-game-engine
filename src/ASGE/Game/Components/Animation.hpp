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
 * @brief Frame-by-frame playback state for a spritesheet.
 *
 * systems::AnimationSystem steps m_CurrentFrame and writes the current frame
 * of m_Clip into the owning entity's Sprite::m_SourceRect each run.
 *
 * Frames live in m_Clip, a shared VFS-loaded asset::FrameTable. Only
 * m_ClipPath is serialized to TOML; m_Clip stays null until
 * asset::AssetManager::ResolveAssets resolves it. m_ResolvedClipPath
 * (runtime-only) records what m_Clip was last resolved from, so
 * Resolver<Animation> can detect a repointed or cleared path.
 *
 * Ownership: while playing, the Animation owns Sprite::m_SourceRect. The
 * Sprite's own crop is saved in m_RestRect on start and restored when
 * m_Playing goes false (one run later) or m_ClipPath is cleared. With
 * m_FinishBehavior == HoldLastFrame, the last frame is kept instead, until
 * the next clip starts or m_ClipPath is cleared.
 *
 * Removing the Animation mid-playback is the caller's job: write m_RestRect
 * back to the Sprite first, or it keeps the last frame's rect (the level
 * editor's Remove handler does this).
 */
struct Animation
{
    using frame_table = std::shared_ptr<asset::Asset<asset::FrameTable>>;

    // Serialized fields
    std::map<str::String, str::String, std::less<>> m_Clips{}; // Named clips (idle/run/...)
    std::map<str::String, float, std::less<>>       m_ClipFrameDurations{}; // Optional per-clip frame duration (seconds)
    
    str::String     m_ClipPath      {};     // Virtual path of the clip's FrameTable TOML meta-file
    float           m_FrameDuration {0.1f}; // Seconds each frame is shown before advancing
    float           m_PlaybackSpeed {1.0f}; // Multiplier on playback rate
    bool            m_Loop          {true}; // Serialized. Wrap to frame 0 at the end instead of stopping there
    FinishBehavior  m_FinishBehavior{FinishBehavior::RestoreSprite}; // What a finished non-looping clip leaves on the Sprite
    bool            m_StartPlaying  {true};  // Initial m_Playing on load
    
    // Runtime only values
    frame_table                 m_Clip              {};      // Resolved FrameTable asset
    str::String                 m_ResolvedClipPath  {};      // The path m_Clip was actually last resolved from
    std::size_t                 m_CurrentFrame      {0};     // index into m_Clip's FrameTable::m_Frames
    float                       m_ElapsedTime       {0.0f};  // seconds accumulated toward the next frame advance
    bool                        m_Playing           {true};  // whether AnimationSystem advances this Animation at all
    std::optional<math::Rect>   m_RestRect          {};      // the Sprite's own m_SourceRect, saved while this Animation owns it
    bool                        m_OwnsSourceRect    {false}; // true between playback starting and the Sprite's crop being restored
    bool                        m_HoldingLastFrame  {false}; // true while a finished HoldLastFrame clip's last frame is kept on the Sprite
};

/**
 * @brief Resumes/starts playback from the current frame.
 *
 * Does not reset m_CurrentFrame or m_ElapsedTime, so calling this on an
 * already in-progress Animation continues rather than restarts it. inLoop
 * becomes the new m_Loop.
 */
void PlayAnimation( Animation& inAnim, bool inLoop = true ) noexcept;

/**
 * @brief Switches to the clip registered as inName in inAnim.m_Clips.
 *
 * Points m_ClipPath at it and restarts playback from frame 0, looping or not
 * per inLoop. Does nothing if that clip is already playing, so calling it
 * every frame is safe. Returns false for an unknown name.
 */
bool PlayClip( Animation& inAnim, str::StringView inName, bool inLoop = true ) noexcept;

/**
 * @brief The fallback seconds-per-frame for inAnim's current clip.
 *
 * The m_ClipFrameDurations override of a named clip whose path is m_ClipPath
 * if it has a positive one, else m_FrameDuration. A per-frame entry in the
 * clip's FrameTable still takes precedence over this.
 */
[[nodiscard]] float EffectiveFrameDuration( Animation const& inAnim ) noexcept;

/**
 * @brief Pauses playback in place.
 *
 * AnimationSystem stops advancing this Animation until PlayAnimation is
 * called again.
 */
void StopAnimation( Animation& inAnim ) noexcept;

}

namespace asge::game::scene
{

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
