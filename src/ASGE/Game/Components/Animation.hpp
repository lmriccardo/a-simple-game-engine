#pragma once

#include <vector>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief Frame-by-frame playback state for a spritesheet — see
 *        systems::AnimationSystem, which steps m_CurrentFrame and writes it
 *        into the owning entity's Sprite::m_SourceRect each frame.
 *
 * m_Frames is independent of the texture itself; each entry is a sub-rect
 * (in texture pixels) of whatever texture the entity's Sprite currently
 * points at — see graphics::MakeGridFrames for building one from a regular
 * grid spritesheet.
 */
struct Animation
{
    std::vector<math::Rect> m_Frames{};          // Sub-rects of the Sprite's texture, one per frame, in playback order
    float                   m_FrameDuration{0.1f}; // Seconds each frame is shown before advancing
    std::size_t             m_CurrentFrame{0};    // Index into m_Frames currently written to Sprite::m_SourceRect
    float                   m_ElapsedTime{0.0f};  // Seconds accumulated toward the next frame advance
    bool                    m_Loop{true};         // Wrap to frame 0 at the end instead of stopping there
    bool                    m_Playing{true};       // Whether AnimationSystem advances this Animation at all
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
 * @brief Round-trips m_Frames and m_FrameDuration only — the animation's
 *        definition, not its live playback progress. FromToml leaves
 *        m_CurrentFrame/m_ElapsedTime/m_Loop/m_Playing at Animation's
 *        in-code defaults; a scene file describes what an entity's
 *        animation is, not where a previous run happened to leave it.
 */
template<>
struct Serializer<Animation>
{
    static constexpr str::StringView kTableName = "Animation";

    using T = Animation;

    static void ToToml( T inValue, asge::config::TOMLTableView inTview ) noexcept
    {
        auto table = inTview.Table( str::String(kTableName) );
        table.Set( "m_FrameDuration", inValue.m_FrameDuration );
        table.Set( "m_NofFrames", static_cast<int>(inValue.m_Frames.size()) );
        for ( auto const& frame : inValue.m_Frames )
        {
            auto frameTable = table.ArrayTable( "Frame" );
            Serializer<math::Rect>::ToToml( frame, frameTable );
        }
    }

    static T FromToml( asge::config::TOMLTableView inTview ) noexcept
    {
        auto table = inTview.Table(std::string(kTableName));

        Animation result{};
        result.m_FrameDuration = table.Get( "m_FrameDuration", 0.1f );

        auto const nofFrames = static_cast<std::size_t>(table.Get<int>( "m_NofFrames", 0 ));
        for ( std::size_t ii{0}; ii < nofFrames; ++ii )
        {
            auto frame = table.GetTable( "Frame", static_cast<int>(ii) );
            if ( !frame ) break;
            result.m_Frames.push_back(
                Serializer<math::Rect>::FromToml( frame.Value() ) );
        }

        return result;
    }
};

}
