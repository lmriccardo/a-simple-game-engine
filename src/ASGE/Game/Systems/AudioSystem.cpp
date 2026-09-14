#include "AudioSystem.hpp"

#include <ASGE/Game/Components/AudioSource.hpp>

namespace
{

using namespace asge::game::components;
using namespace asge::audio;

/** @brief Creates inSource's stream on first use, then (re)fills it with its clip's PCM data. */
void StartOrReplaySource( AudioDevice &inDevice, AudioSource& inSource )
{
    // If the stream is nullpointer then we need to create one
    if ( !inSource.m_Stream )
    {
        auto streamResult = inDevice.CreateStream( inSource.m_Clip->Get() );
        if ( !streamResult ) { streamResult.LogError(); return; }
        inSource.m_Stream = streamResult.Value();
    }

    // Check for stream validity
    if ( !inSource.m_Stream->IsValid() ) return;
    inSource.m_Stream->ClearData();
    inSource.m_Stream->SetAudioGain( inSource.m_Volume );
    inSource.m_Stream->PutData( inSource.m_Clip->Get() );
}

/** @brief Clears inSource's queued PCM data, if it has a live stream. */
void StopSource( AudioSource& inSource )
{
    if ( inSource.m_Stream && inSource.m_Stream->IsValid() )
    {
        inSource.m_Stream->ClearData();
    }
}

}

void asge::game::systems::AudioSystem(ecs::Registry &inRegistry, audio::AudioDevice &inDevice)
{
    for ( auto [ entity, source ] : inRegistry.View<components::AudioSource>() )
    {
        auto& s = source.get();

        if ( s.m_Stream != nullptr && !s.m_Stream->IsValid() ) continue;

        if ( s.m_Playing && s.m_Clip && ( !s.m_Stream || s.m_Restart ) )
        {
            StartOrReplaySource( inDevice, s );
            s.m_Restart = false;
        }

        if ( s.m_Stream && !s.m_Stream->IsDataAvailable() )
        {
            if ( s.m_Loop ) StartOrReplaySource( inDevice, s );
            else {
                s.m_Playing = false;
            }
        }

        if ( !s.m_Playing && s.m_Stream ) StopSource( s );
    }
}