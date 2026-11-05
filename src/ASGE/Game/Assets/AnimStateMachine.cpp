#include "AnimStateMachine.hpp"

#include <algorithm>

int asge::game::asset::AnimationStateMachine::GetIndexFrom(
    std::vector<str::String> const &inVec, str::StringView inVal) noexcept
{
    for ( std::size_t ii = 0; ii < inVec.size(); ++ii )
    {
        if ( inVec[ii] == inVal ) return ii;
    }
    return -1;
}

asge::Result<std::uint16_t> asge::game::asset::AnimationStateMachine::FindState(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_StateNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint16_t>::Err( 
            make_error_code(errors::AnimStateMachineErrors::StateNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint16_t>::Ok( static_cast<std::uint16_t>(idx) );
}

asge::Result<std::uint8_t> asge::game::asset::AnimationStateMachine::FindFlag(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_FlagNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint8_t>::Err( 
            make_error_code(errors::AnimStateMachineErrors::FlagNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint8_t>::Ok( static_cast<std::uint8_t>(idx) );
}

asge::Result<std::uint8_t> asge::game::asset::AnimationStateMachine::FindTrigger(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_TriggerNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint8_t>::Err( 
            make_error_code(errors::AnimStateMachineErrors::TriggerNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint8_t>::Ok( static_cast<std::uint8_t>(idx) );
}
