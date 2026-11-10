#include "Animator.hpp"

#include <cmath>

void asge::game::scene::Serializer<asge::game::components::Animator>::ToToml(
    T inValue, asge::formats::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set<str::String>( "m_ClipSetPath", inValue.m_ClipSetPath )
           .Set( "m_PlaybackSpeed", inValue.m_PlaybackSpeed )
           .Set( "m_StartPlaying", inValue.m_StartPlaying );
}

asge::game::components::Animator asge::game::scene::Serializer<asge::game::components::Animator>::FromToml(
    asge::formats::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    T result{};
    result.m_ClipSetPath   = table.Get( "m_ClipSetPath", str::String{} );
    result.m_PlaybackSpeed = table.Get( "m_PlaybackSpeed", result.m_PlaybackSpeed );
    result.m_StartPlaying  = table.Get( "m_StartPlaying", result.m_StartPlaying );
    return result;
}

bool asge::game::components::SetFlag(
    Animator &ioAnimator, str::StringView inFlagName, bool inValue) noexcept
{
    // Get Flag position if any from the state machine
    if ( ioAnimator.m_ClipSet == nullptr ) return false;
    auto const& machine = ioAnimator.m_ClipSet->Get().Machine();
    auto idxResult = machine.FindFlag( inFlagName );
    if ( !idxResult )
    {
        idxResult.LogError();
        return false;
    }

    std::uint32_t const bit = 1u << idxResult.Value();
    std::uint32_t const next = inValue ? ( ioAnimator.m_Flags | bit ) 
        : ( ioAnimator.m_Flags & ~bit );

    if ( next != ioAnimator.m_Flags )
    {
        ioAnimator.m_Flags = next;
        ioAnimator.m_Dirty = true;
    }

    return true;
}

bool asge::game::components::Fire(
    Animator &ioAnimator, str::StringView inTriggerName) noexcept
{
    if ( ioAnimator.m_ClipSet == nullptr ) return false;
    auto const& machine = ioAnimator.m_ClipSet->Get().Machine();
    auto idxResult = machine.FindTrigger( inTriggerName );
    if ( !idxResult )
    {
        idxResult.LogError();
        return false;
    }

    ioAnimator.m_Triggers |= 1u << idxResult.Value();
    ioAnimator.m_Dirty = true;
    return true;
}

void asge::game::components::SetHeading( Animator& ioAnimator, math::Float2 inHeading ) noexcept
{
    if ( !std::isfinite( inHeading.x() ) || !std::isfinite( inHeading.y() ) ) return;
    if ( inHeading.x() == 0.0f && inHeading.y() == 0.0f ) return;

    ioAnimator.m_Heading = inHeading;
}

void asge::game::components::Stop(Animator &ioAnimator) noexcept
{
    if ( ioAnimator.m_Status == AnimStatus::Playing ) 
    {
        ioAnimator.m_Status = AnimStatus::Stopped;
    }
}

void asge::game::components::Resume(Animator &ioAnimator) noexcept
{
    if ( ioAnimator.m_Status == AnimStatus::Stopped ) 
    {
        ioAnimator.m_Status = AnimStatus::Playing;
    }
}

bool asge::game::components::IsFinished(Animator const &inAnimator) noexcept
{
    return inAnimator.m_Status == AnimStatus::Finished;
}
