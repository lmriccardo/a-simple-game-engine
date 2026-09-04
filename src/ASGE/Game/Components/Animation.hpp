#pragma once

#include <memory>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>
#include "Serialize.hpp"

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
 */
struct Animation
{
    using frame_table = std::shared_ptr<asset::Asset<asset::FrameTable>>;

    str::String m_ClipPath{}; // Virtual path of the clip's FrameTable TOML meta-file
    frame_table m_Clip{};     // Resolved FrameTable asset -- null until AssetManager::ResolveAssets runs

    float       m_FrameDuration{0.1f}; // Seconds each frame is shown before advancing
    std::size_t m_CurrentFrame{0};     // Index into m_Clip's FrameTable::m_Frames currently written to Sprite::m_SourceRect
    float       m_ElapsedTime{0.0f};   // Seconds accumulated toward the next frame advance
    bool        m_Loop{true};          // Wrap to frame 0 at the end instead of stopping there
    bool        m_Playing{true};       // Whether AnimationSystem advances this Animation at all
};

/**
 * @brief Resumes/starts playback from the current frame — does not reset
 *        m_CurrentFrame or m_ElapsedTime, so calling this on an already
 *        in-progress Animation continues rather than restarts it.
 */
inline void PlayAnimation( Animation& inAnim, bool inLoop = true ) noexcept
{
    inAnim.m_Playing = true;
    inAnim.m_Loop = inLoop;
}

/** @brief Pauses playback in place; AnimationSystem stops advancing this Animation until PlayAnimation is called again. */
inline void StopAnimation( Animation& inAnim ) noexcept
{
    inAnim.m_Playing = false;
}

/**
 * @brief Round-trips m_ClipPath and m_FrameDuration only — which clip to
 *        play and how fast, not the live playback progress. FromToml leaves
 *        m_Clip null (resolved later by AssetManager::ResolveAssets, same
 *        as Sprite::m_Texture) and m_CurrentFrame/m_ElapsedTime/m_Loop/
 *        m_Playing at Animation's in-code defaults; a scene file describes
 *        what an entity's animation is, not where a previous run happened
 *        to leave it.
 */
template<>
struct Serializer<Animation>
{
    static constexpr str::StringView kTableName = "Animation";

    using T = Animation;

    static void ToToml( T inValue, asge::config::toml::TOMLTableView inTview ) noexcept
    {
        inTview.Table(std::string(kTableName))
               .Set<std::string>("m_ClipPath", inValue.m_ClipPath)
               .Set("m_FrameDuration", inValue.m_FrameDuration);
    }

    static T FromToml( asge::config::toml::TOMLTableView inTview ) noexcept
    {
        auto table = inTview.Table(std::string(kTableName));

        Animation result{};
        result.m_ClipPath      = table.Get<std::string>("m_ClipPath", std::string{});
        result.m_FrameDuration = table.Get("m_FrameDuration", result.m_FrameDuration);
        return result;
    }
};

}
