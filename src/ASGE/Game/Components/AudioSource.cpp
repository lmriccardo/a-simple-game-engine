#include "AudioSource.hpp"

void asge::game::components::PlayAudioSource( AudioSource& inAudioSource, bool inLoop ) noexcept
{
    inAudioSource.m_Playing = true;
    inAudioSource.m_Loop = inLoop;
    inAudioSource.m_Restart = true;
}

void asge::game::components::StopAudioSource( AudioSource& inAudioSource ) noexcept
{
    inAudioSource.m_Playing = false;
}

asge::BoolResult asge::game::components::DetachAudioSource(
    audio::AudioDevice& inDevice, AudioSource& inAudioSource ) noexcept
{
    if ( !inAudioSource.m_Stream ) return BoolResult::Ok();

    auto result = inDevice.DetachStream( *inAudioSource.m_Stream );
    if ( !result ) return result;

    inAudioSource.m_Stream = nullptr;
    inAudioSource.m_Playing = false;
    return BoolResult::Ok();
}

void asge::game::components::SetVolume( AudioSource& inAudioSource, float inVolume ) noexcept
{
    inAudioSource.m_Volume = inVolume;
}

void asge::game::components::Serializer<asge::game::components::AudioSource>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set<str::String>( "m_VirtualClipPath", inValue.m_VirtualClipPath );
}

asge::game::components::AudioSource asge::game::components::Serializer<asge::game::components::AudioSource>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    AudioSource result;
    result.m_VirtualClipPath = table.Get("m_VirtualClipPath", str::String{});
    return result;
}
