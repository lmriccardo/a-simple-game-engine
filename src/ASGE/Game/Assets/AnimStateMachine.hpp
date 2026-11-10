#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>
#include "FrameTable.hpp"
#include "Asset.hpp"

BEGIN_ASGE_ERROR_NS

enum class AnimStateMachineErrors
{
    StateNotFound,
    FlagNotFound,
    TriggerNotFound,
    ClipNotFound,
    MissingInitialState,
    UnexpectedElement,
    MissingAttribute,
    EmptyAttribute,
    InvalidAttribute,
    DuplicateName,
    LimitExceeded
};

inline str::String ToErrorString(AnimStateMachineErrors e) noexcept
{
    switch (e)
    {
    case AnimStateMachineErrors::StateNotFound: return "input state not found";
    case AnimStateMachineErrors::FlagNotFound: return "input flag not found";
    case AnimStateMachineErrors::TriggerNotFound: return "input trigger not found";
    case AnimStateMachineErrors::ClipNotFound: return "input clip not found";
    case AnimStateMachineErrors::MissingInitialState: return "initial state is missing";
    case AnimStateMachineErrors::UnexpectedElement: return "unexpected xml element";
    case AnimStateMachineErrors::MissingAttribute: return "required attribute is missing";
    case AnimStateMachineErrors::EmptyAttribute: return "attribute value is empty";
    case AnimStateMachineErrors::InvalidAttribute: return "attribute value is invalid";
    case AnimStateMachineErrors::DuplicateName: return "name is already in use";
    case AnimStateMachineErrors::LimitExceeded: return "too many flags, triggers, clips or states";
    }
    return "unknown state machine error";
}

END_ASGE_ERRORS_NS

REGISTER_ASGE_ERROR( asge::errors::AnimStateMachineErrors, "asge.game.asset" );

namespace asge::game::asset
{

enum class EndMode : std::uint8_t { Loop, Hold, Restore };

class AnimationStateMachine
{
public:
    static constexpr std::size_t kMaxFlags    = 32;  // bits in Animator::m_Flags
    static constexpr std::size_t kMaxTriggers = 32;  // bits in Animator::m_Triggers
    static constexpr std::size_t kMaxStates   = std::numeric_limits<std::uint16_t>::max();

    struct State
    {
        std::uint16_t   m_ClipId            {};
        EndMode         m_End               {EndMode::Loop};
        std::uint32_t   m_FirstTransition   {}; // CSR slice into m_Transitions
        std::uint32_t   m_TransitionCount   {};
    };

    struct Transition
    {
        std::uint16_t   m_To         {};
        std::uint32_t   m_FlagsSet   {0}; // all these bits must be set
        std::uint32_t   m_FlagsClear {0}; // all these bits must be clear
        std::uint32_t   m_Trigger    {0}; // at most one bit; 0 = no trigger. Consumed when taken
        bool            m_AtEnd      {false};
    };
private:
    std::vector<State>      m_States        {};
    std::vector<Transition> m_Transitions   {}; // grouped by from-state, each slice sorted by priority desc
    std::uint16_t           m_Initial       {0};

    // These fields are setted up once at load time and never touched again
    std::vector<str::String> m_StateNames   {};
    std::vector<str::String> m_ClipNames    {};
    std::vector<str::String> m_FlagNames    {};
    std::vector<str::String> m_TriggerNames {};

    static int GetIndexFrom( std::vector<str::String> const& inVec, str::StringView inVal ) noexcept;
public:
    AnimationStateMachine() = default; // Only the default constructor

    /**
     * @brief Loads a state machine from an XML file.
     *
     * The root is `<AnimStateMachine initial="...">` with optional `<Flags>`,
     * `<Triggers>` and `<Clips>` lists and one or more `<State name clip [end]>`
     * elements holding `<Transition to [trigger] [when] [atEnd] [priority]>`.
     * A state's clip names an entry of `<Clips>`; the paths and frame durations
     * come from an AnimationClipSet. Missing, blank or malformed attributes,
     * unknown names, duplicates and more than 32 flags or triggers return a
     * specific AnimStateMachineErrors.
     */
    [[nodiscard]] static Result<AnimationStateMachine> Load( filesystem::Path const& inPath );

    [[nodiscard]] std::span<State const> States() const noexcept;
    [[nodiscard]] std::span<Transition const> TransitionsOf( std::uint16_t inState ) const noexcept;

    [[nodiscard]] std::uint16_t Initial() const noexcept;

    /** @brief Returns the clip names in id order, i.e. the position is State::m_ClipId. */
    [[nodiscard]] std::span<str::String const> ClipNames() const noexcept;

    [[nodiscard]] Result<std::uint16_t> FindState( str::StringView inName ) const noexcept;
    [[nodiscard]] Result<std::uint8_t>  FindFlag( str::StringView inName ) const noexcept;
    [[nodiscard]] Result<std::uint8_t>  FindTrigger( str::StringView inName ) const noexcept;
    /**
     * @brief Returns the id of a clip declared in `<Clips>`.
     *
     * Fails with ClipNotFound. The id is the clip's position in the list and
     * the value stored in State::m_ClipId.
     */
    [[nodiscard]] Result<std::uint16_t> FindClip( str::StringView inName ) const noexcept;
};

}