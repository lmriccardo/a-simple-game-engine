#pragma once

#include <ASGE/Game/Assets/AnimationClipSet.hpp>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Strings.hpp>

namespace asge::game::components
{

enum class AnimStatus : std::uint8_t { Playing, Paused, Finished };

using FlagId    = std::uint8_t;
using TriggerId = std::uint8_t;

/**
 * @brief State-machine driven sprite animation.
 *
 * Plays the clips of an AnimationStateMachine through an AnimationClipSet,
 * which binds the machine's clip names to frame tables. Game code never
 * touches the current state: it sets persistent conditions with SetFlag()
 * and one-shot events with Fire(), and the machine's transitions pick the
 * next state from them.
 *
 * Only m_ClipSetPath, m_PlaybackSpeed and m_StartPlaying are serialized.
 * m_ClipSet stays null until asset::AssetManager resolves the clip set, and
 * SetFlag()/Fire() do nothing before that. Flags and triggers are bit masks
 * indexed by the machine's flag and trigger ids.
 */
struct Animator
{
    using clip_set = std::shared_ptr<asset::Asset<asset::AnimationClipSet>>;

    // Serialized fields
    str::String m_ClipSetPath   {};     // Virtual path of the AnimClipSet XML
    float       m_PlaybackSpeed {1.0f}; // Multiplier on playback rate
    bool        m_StartPlaying  {true}; // Initial status on first entry.

    // Runtime only fields
    float          m_Elapsed   {0.0f};   // seconds accumulated toward the next frame advance
    std::uint32_t  m_Frame     {0};      // index into the current clip's FrameTable::m_Frames
    std::uint32_t  m_Flags     {0};      // persistent conditions, set by game code
    std::uint32_t  m_Triggers  {0};      // pending one-shots, consumed by transitions
    std::uint16_t  m_State     {0};      // current state id in the machine
    AnimStatus     m_Status    {AnimStatus::Playing};
    bool           m_Dirty     {true};   // re-evaluate transitions next update
    bool           m_Started   {false};  // false until the system enters the initial state
    math::Rect     m_RestRect  {};       // Sprite's own source rect, captured on first entry
    clip_set       m_ClipSet   {};       // owns the clip set (and through it the machine)
};

/**
 * @brief Sets or clears a named flag of the animator's state machine.
 *
 * Flags are persistent conditions: they keep their value until set again.
 * The animator is marked dirty only when the value actually changes, so
 * calling this every frame is cheap. Returns false, leaving the animator
 * untouched, if the clip set is not resolved yet or the machine has no such
 * flag (the error is logged).
 */
bool SetFlag( Animator& ioAnimator, str::StringView inFlagName, bool inValue ) noexcept;

/**
 * @brief Raises a named trigger of the animator's state machine.
 *
 * Triggers are one-shot events: the bit stays pending until a transition
 * that listens to it is taken and consumes it. Firing an already pending
 * trigger has no further effect. Returns false, leaving the animator
 * untouched, if the clip set is not resolved yet or the machine has no such
 * trigger (the error is logged).
 */
bool Fire( Animator& ioAnimator, str::StringView inTriggerName ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::Animator>
{
    static constexpr str::StringView kTableName = "Animator";

    using T = components::Animator;

    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
