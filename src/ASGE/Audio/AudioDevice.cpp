#include "AudioDevice.hpp"

#include <SDL3/SDL_init.h>

asge::BoolResult asge::audio::AudioDevice::Initialize()
{
    if ( !SDL_Init( SDL_INIT_AUDIO ) )
    {
        return BoolResult::Err( make_error_code(errors::AudioError::SubsystemInitFailed), SDL_GetError() );
    }
    
    m_BackendInitialized = true;

    SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, 48000 };
    m_Stream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr );
    if ( !m_Stream )
    {
        Shutdown();
        return BoolResult::Err( make_error_code(errors::AudioError::DeviceOpenFailed), SDL_GetError() );
    }

    SDL_ResumeAudioStreamDevice( m_Stream );
    return BoolResult::Ok();
}

void asge::audio::AudioDevice::Shutdown()
{
    if ( m_Stream ) SDL_DestroyAudioStream( m_Stream ); 
    if ( m_BackendInitialized )
    { 
        SDL_QuitSubSystem(SDL_INIT_AUDIO); 
        m_BackendInitialized = false; 
    }
    m_Stream = nullptr;
}

SDL_AudioStream *asge::audio::AudioDevice::Stream() const noexcept
{
    return m_Stream;
}
