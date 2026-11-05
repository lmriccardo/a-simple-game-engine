#pragma once

#include <cstdint>
#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>
#include "FrameTable.hpp"
#include "Asset.hpp"

BEGIN_ASGE_ERROR_NS

enum class AnimStateMachineErrors
{
    StateNotFound,
    FlagNotFound,
    TriggerNotFound
};

inline str::String ToErrorString(AnimStateMachineErrors e) noexcept
{
    switch (e)
    {
    case AnimStateMachineErrors::StateNotFound: return "input state not found";
    case AnimStateMachineErrors::FlagNotFound: return "input flag not found";
    case AnimStateMachineErrors::TriggerNotFound: return "input trigger not found";
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
    struct State
    {
        using asset_type = std::shared_ptr<Asset<FrameTable>>;

        asset_type      m_Clip              {};
        float           m_FrameDuration     {0.1f};
        EndMode         m_End               {EndMode::Loop};
        std::uint32_t   m_FirstTransition   {};
        std::uint32_t   m_TransitionCount   {};
    };

    struct Transition
    {
        std::uint16_t   m_To        {};
        std::uint32_t   m_Flags     {0};
        std::uint32_t   m_Triggers  {0};
        bool            m_AtEnd     {false};
    };
private:
    std::vector<State>      m_States        {};
    std::vector<Transition> m_Transitions   {};

    // These fields are setted up once at load time and never touched again
    std::vector<str::String> m_StateNames   {};
    std::vector<str::String> m_FlagNames    {};
    std::vector<str::String> m_TriggerNames {};

    static int GetIndexFrom( std::vector<str::String> const& inVec, str::StringView inVal ) noexcept;
public:
    AnimationStateMachine() = default; // Only the default constructor

    [[nodiscard]] Result<std::uint16_t> FindState( str::StringView inName ) const noexcept;
    [[nodiscard]] Result<std::uint8_t>  FindFlag( str::StringView inName ) const noexcept;
    [[nodiscard]] Result<std::uint8_t>  FindTrigger( str::StringView inName ) const noexcept;
};

}