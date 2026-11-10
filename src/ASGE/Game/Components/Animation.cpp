#include "Animation.hpp"
#include <algorithm>

void asge::game::components::PlayAnimation( Animation& inAnim, bool inLoop ) noexcept
{
    inAnim.m_Playing = true;
    inAnim.m_Loop = inLoop;
}

bool asge::game::components::PlayClip( Animation& inAnim, str::StringView inName, bool inLoop ) noexcept
{
    auto const it = inAnim.m_Clips.find( inName );
    if ( it == inAnim.m_Clips.end() ) return false;

    if ( inAnim.m_Playing && inAnim.m_ClipPath == it->second ) return true;

    // Reset here, not only in Resolver<Animation>, so the frame is in range for the new clip straight away.
    inAnim.m_ClipPath = it->second;
    inAnim.m_CurrentFrame = 0;
    inAnim.m_ElapsedTime = 0.0f;
    inAnim.m_Loop = inLoop;
    inAnim.m_Playing = true;
    return true;
}

float asge::game::components::EffectiveFrameDuration( Animation const& inAnim ) noexcept
{
    for ( auto const& [name, path] : inAnim.m_Clips )
    {
        if ( path != inAnim.m_ClipPath ) continue;
        auto const it = inAnim.m_ClipFrameDurations.find( name );
        if ( it != inAnim.m_ClipFrameDurations.end() && it->second > 0.0f ) return it->second;
    }
    return inAnim.m_FrameDuration;
}

void asge::game::components::StopAnimation( Animation& inAnim ) noexcept
{
    inAnim.m_Playing = false;
}

void asge::game::scene::Serializer<asge::game::components::Animation>::ToToml(
    T inValue, asge::formats::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    std::vector<std::string> clipNames, clipPaths; // parallel arrays: TOMLTableView can't enumerate a table's keys
    for ( auto const& [name, path] : inValue.m_Clips )
    {
        clipNames.push_back( name );
        clipPaths.push_back( path );
    }

    // Parallel arrays again; only overrides for a name that still exists in m_Clips are written.
    std::vector<std::string> durationNames;
    std::vector<double> durationValues;
    for ( auto const& [name, seconds] : inValue.m_ClipFrameDurations )
    {
        if ( !inValue.m_Clips.contains( name ) ) continue;
        durationNames.push_back( name );
        durationValues.push_back( seconds );
    }

    auto table = inTview.Table(std::string(kTableName));
    if ( !durationNames.empty() ) table.SetArray<std::string>("m_ClipDurationNames", durationNames).SetArray<double>("m_ClipDurationValues", durationValues);
    if ( !clipNames.empty() ) table.SetArray<std::string>("m_ClipNames", clipNames).SetArray<std::string>("m_ClipPaths", clipPaths);
    table.Set<std::string>("m_ClipPath", inValue.m_ClipPath)
           .Set("m_FrameDuration", inValue.m_FrameDuration)
           .Set("m_PlaybackSpeed", inValue.m_PlaybackSpeed)
           .Set("m_Loop", inValue.m_Loop)
           .Set<std::string>("m_FinishBehavior", inValue.m_FinishBehavior == components::FinishBehavior::HoldLastFrame ? "HoldLastFrame" : "RestoreSprite")
           .Set("m_StartPlaying", inValue.m_StartPlaying);
}

asge::game::components::Animation asge::game::scene::Serializer<asge::game::components::Animation>::FromToml(
    asge::formats::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table(std::string(kTableName));

    T result{};
    result.m_ClipPath      = table.Get<std::string>("m_ClipPath", std::string{});

    auto const clipNames = table.GetArray<std::string>("m_ClipNames");
    auto const clipPaths = table.GetArray<std::string>("m_ClipPaths");
    for ( std::size_t i = 0; i < std::min( clipNames.size(), clipPaths.size() ); ++i )
    {
        result.m_Clips[clipNames[i]] = clipPaths[i];
    }

    auto const durationNames = table.GetArray<std::string>("m_ClipDurationNames");
    auto const durationValues = table.GetArray<double>("m_ClipDurationValues");
    for ( std::size_t i = 0; i < std::min( durationNames.size(), durationValues.size() ); ++i )
    {
        result.m_ClipFrameDurations[durationNames[i]] = static_cast<float>( durationValues[i] );
    }

    result.m_FrameDuration = table.Get("m_FrameDuration", result.m_FrameDuration);
    result.m_PlaybackSpeed = table.Get("m_PlaybackSpeed", result.m_PlaybackSpeed);
    result.m_Loop          = table.Get("m_Loop", result.m_Loop);
    if ( table.Get<std::string>("m_FinishBehavior", std::string{}) == "HoldLastFrame" )
    {
        result.m_FinishBehavior = components::FinishBehavior::HoldLastFrame;
    }

    result.m_StartPlaying  = table.Get("m_StartPlaying", result.m_StartPlaying);
    result.m_Playing       = result.m_StartPlaying;
    return result;
}
