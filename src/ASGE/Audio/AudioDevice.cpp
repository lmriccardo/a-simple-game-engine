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
    LOG_DEBUG( "New audio device ", SDL_GetAudioDeviceName(m_DeviceId), " opened" );
    return BoolResult::Ok();
}

void asge::audio::AudioDevice::Shutdown()
{
    // First we need to destroy all streams
    for ( auto& i_stream : m_Streams )
    {
        if ( !i_stream ) continue; // unused slot, nothing bound yet

        // Unbind from the audio device
        SDL_UnbindAudioStream( i_stream->Get() );

        // We need to reset the shared pointer, this method will also
        // calls the destructor of the pointer itself
        i_stream->Reset();
    }
    m_LastIndex = 0;

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

asge::Result<std::shared_ptr<asge::audio::AudioStream>> 
asge::audio::AudioDevice::CreateStream( media::AudioClip& inAudioClip ) noexcept
{
    if ( !m_BackendInitialized )
    {
        // First we need to check that the backend audio system is initialized
        return Result<std::shared_ptr<asge::audio::AudioStream>>::Err(
            make_error_code( errors::AudioError::SubsystemNotInitialized ));
    }

    if ( m_LastIndex == kMaxNofStreams )
    {
        return Result<std::shared_ptr<asge::audio::AudioStream>>::Err(
            make_error_code( errors::AudioError::StreamCreationFailed ),
            "no more space for new streams for device ID " + 
            std::to_string( m_DeviceId )
        );
    }

    SDL_AudioSpec const& clipSpec = inAudioClip.Spec();
    SDL_AudioStream* currStream = SDL_CreateAudioStream( &clipSpec, nullptr );
    if ( !currStream || !SDL_BindAudioStream( m_DeviceId, currStream ) )
    {
        return Result<std::shared_ptr<asge::audio::AudioStream>>::Err(
            make_error_code( errors::AudioError::StreamCreationFailed ),
            SDL_GetError()
        );
    }

    auto stream = std::shared_ptr<SDL_AudioStream>( currStream, SDL_DestroyAudioStream );
    std::size_t const index = m_LastIndex++;
    m_Streams[index] = std::make_shared<AudioStream>(std::move(stream), index);

    SDL_FlushAudioStream( currStream );
    return Result<std::shared_ptr<asge::audio::AudioStream>>::Ok( m_Streams[index] );
}

asge::BoolResult asge::audio::AudioDevice::DetachStream(AudioStream& inStream) noexcept
{
    if ( !m_BackendInitialized )
    {
        // First we need to check that the backend audio system is initialized
        return BoolResult::Err(
            make_error_code( errors::AudioError::SubsystemNotInitialized ));
    }

    // First we need to check that the input stream is still valid
    if ( !inStream.IsValid() || inStream.Index() >= m_LastIndex )
    {
        return BoolResult::Err( make_error_code( errors::AudioError::InvalidStream ) );
    }

    // First we need to check that the stream is on this device
    if ( auto devId = SDL_GetAudioStreamDevice( inStream.Get() ); devId != m_DeviceId )
    {
        return BoolResult::Err( 
            make_error_code( errors::AudioError::InvalidStream ),
            "stream not bind to device " + std::to_string( m_DeviceId )
        );
    }

    // Unbind the audio stream from the device and reset its pointer
    std::size_t const currIndex = inStream.Index();
    SDL_UnbindAudioStream( inStream.Get() );
    inStream.Reset();
    inStream.Index( static_cast<std::size_t>(-1) );

    // Swap-remove: move the trailing slot into the freed one (a no-op if
    // currIndex was already the trailing slot) and shrink by one. Whoever
    // holds the relocated stream keeps the same shared_ptr<AudioStream>
    // regardless of which array slot backs it, but its own bookkeeping still
    // needs to know its new slot -- otherwise a later DetachStream() call on
    // it would compare its stale Index() against the shrunk m_LastIndex and
    // wrongly report it as already detached.
    m_Streams[currIndex] = std::move( m_Streams[m_LastIndex - 1] );
    m_Streams[m_LastIndex - 1] = nullptr;
    --m_LastIndex;

    if ( m_Streams[currIndex] ) m_Streams[currIndex]->Index( currIndex );

    return BoolResult::Ok();
}

asge::BoolResult asge::audio::AudioDevice::SetGain(float inVolume) noexcept
{
    if ( !m_BackendInitialized )
    {
        // First we need to check that the backend audio system is initialized
        return BoolResult::Err(make_error_code( errors::AudioError::SubsystemNotInitialized ));
    }

    if ( !SDL_SetAudioDeviceGain( m_DeviceId, inVolume ) )
    {
        return BoolResult::Err(
            make_error_code( errors::AudioError::InvalidDevice ),
            SDL_GetError()
        );
    }

    return BoolResult::Ok();
}
