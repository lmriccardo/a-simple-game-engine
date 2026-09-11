#include "AudioDevice.hpp"

#include <SDL3/SDL_init.h>
#include <ASGE/Core/Media/AudioClip.hpp>

asge::BoolResult asge::audio::AudioDevice::Initialize()
{
    auto result = asge::media::InitializeAudioSystem();
    if ( !result ) return result;
    m_BackendInitialized = result.Value();

    auto devResult = asge::media::OpenNewAudioDevice();
    if ( !devResult ) { Shutdown(); return BoolResult::Err(devResult.Error()); }
    m_DeviceId = devResult.Value();

    SDL_ResumeAudioDevice( m_DeviceId );
    return BoolResult::Ok();
}

void asge::audio::AudioDevice::Shutdown()
{
    // First we need to destroy all streams
    for ( auto i_stream : m_Streams )
    {
        // Unbind from the audio device
        SDL_UnbindAudioStream( i_stream.get() );

        // We need to reset the shared pointer, this method will also
        // calls the destructor of the pointer itself
        i_stream.reset();
    }

    if ( m_DeviceId ) SDL_CloseAudioDevice( m_DeviceId ); 
    if ( m_BackendInitialized )
    { 
        SDL_QuitSubSystem(SDL_INIT_AUDIO); 
        m_BackendInitialized = false; 
    }
    m_DeviceId = 0;
}

std::size_t asge::audio::AudioDevice::Size() const noexcept
{
    return m_LastIndex;
}

SDL_AudioDeviceID asge::audio::AudioDevice::Id() const noexcept
{
    return m_DeviceId;
}

asge::Result<asge::audio::AudioDevice::stream_tag> 
asge::audio::AudioDevice::CreateStream( media::AudioClip& inAudioClip ) noexcept
{
    if ( !m_BackendInitialized )
    {
        // First we need to check that the backend audio system is initialized
        return Result<stream_tag>::Err(make_error_code( errors::AudioError::SubsystemNotInitialized ));
    }

    if ( m_LastIndex == kMaxNofStreams )
    {
        return Result<stream_tag>::Err(
            make_error_code( errors::AudioError::StreamCreationFailed ),
            "no more space for new streams for device ID " + 
            std::to_string( m_DeviceId )
        );
    }

    SDL_AudioSpec const& clipSpec = inAudioClip.Spec();
    SDL_AudioStream* currStream = SDL_CreateAudioStream( &clipSpec, nullptr );
    if ( !currStream || !SDL_BindAudioStream( m_DeviceId, currStream ) )
    {
        return Result<stream_tag>::Err(
            make_error_code( errors::AudioError::StreamCreationFailed ),
            SDL_GetError()
        );
    }

    m_Streams[m_LastIndex++] = AudioDevice::stream(currStream, SDL_DestroyAudioStream);
    SDL_FlushAudioStream( currStream );
    return Result<stream_tag>::Ok( m_Streams.at( m_LastIndex - 1 ) );
}
