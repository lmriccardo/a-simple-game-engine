#include "AudioClip.hpp"

#include <span>
#include <stb_vorbis.c>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_init.h>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Strings.hpp>

namespace
{

/** @brief Decodes RIFF/WAVE bytes into raw PCM via SDL's own WAV loader. */
asge::Result<asge::media::AudioClip> DecodeWav( std::span<const std::byte> inBytes ) noexcept
{
    SDL_IOStream* io = SDL_IOFromConstMem( inBytes.data(), inBytes.size() );

    SDL_AudioSpec spec{};
    std::uint8_t* buff{ nullptr };
    std::uint32_t len{ 0 };

    if ( !SDL_LoadWAV_IO( io, true, &spec, &buff, &len ) )
    {
        auto const ec = make_error_code( asge::errors::AudioError::DecodeFailed );
        return asge::Result<asge::media::AudioClip>::Err( ec, SDL_GetError() );
    }

    asge::media::AudioClip::data_t data( buff, buff + len ); // copy out of SDL's allocator
    SDL_free( buff );

    return asge::Result<asge::media::AudioClip>::Ok( asge::media::AudioClip( spec, std::move(data) ) );
}

/** @brief Decodes Ogg Vorbis bytes into signed 16-bit PCM via stb_vorbis. */
asge::Result<asge::media::AudioClip> DecodeOgg( std::span<const std::byte> inBytes ) noexcept
{
    int channels = 0, sampleRate = 0;
    short* pcm = nullptr;

    int samples = stb_vorbis_decode_memory(
        reinterpret_cast<const unsigned char*>(inBytes.data()),
        static_cast<int>(inBytes.size()), &channels, &sampleRate, &pcm );

    if ( samples < 0 )
    {
        auto const ec = make_error_code( asge::errors::AudioError::DecodeFailed );
        return asge::Result<asge::media::AudioClip>::Err( ec, "stb_vorbis decode failed" );
    }

    SDL_AudioSpec spec{ SDL_AUDIO_S16, channels, sampleRate };
    auto const byteLen = static_cast<std::size_t>(samples) * channels * sizeof(short);

    asge::media::AudioClip::data_t data(
        reinterpret_cast<std::uint8_t*>(pcm),
        reinterpret_cast<std::uint8_t*>(pcm) + byteLen ); // copy out of stb's allocator
    free( pcm );

    return asge::Result<asge::media::AudioClip>::Ok( asge::media::AudioClip( spec, std::move(data) ) );
}

}

SDL_AudioSpec const &asge::media::AudioClip::Spec() const noexcept
{
    return m_Spec;
}

std::uint8_t const *asge::media::AudioClip::Data() const noexcept
{
    return m_Data.data();
}

std::size_t asge::media::AudioClip::Size() const noexcept
{
    return m_Data.size();
}

asge::Result<asge::media::AudioClip> 
asge::media::AudioClip::Load(filesystem::Path const &inPath) noexcept
{
    auto byteResult = filesystem::ReadBinary( inPath );
    if ( !byteResult ) return Result<AudioClip>::Err( byteResult.Error() );

    if ( inPath.extension() == ".ogg" ) return DecodeOgg( byteResult.Value() );
    if ( inPath.extension() == ".wav" ) return DecodeWav( byteResult.Value() );
    return Result<AudioClip>::Err( 
        make_error_code( errors::AudioError::InvalidFormat ),
        str::ToUTF8( inPath.extension().u8string() )
    );
}

bool asge::media::IsAudioSystemInitialized() noexcept
{
    return ( SDL_WasInit( SDL_INIT_AUDIO ) & SDL_INIT_AUDIO ) == 1;
}

asge::BoolResult asge::media::InitializeAudioSystem() noexcept
{
    // Check If the audio subsystem was already initialized
    if ( !IsAudioSystemInitialized() )
    {
        if ( !SDL_Init( SDL_INIT_AUDIO ) )
        {
            return BoolResult::Err( 
                make_error_code( errors::AudioError::SubsystemInitFailed ),
                SDL_GetError()
            );
        }
    }

    return BoolResult::Ok();
}

asge::Result<SDL_AudioDeviceID> asge::media::OpenNewAudioDevice() noexcept
{
    if ( !IsAudioSystemInitialized() )
    {
        return Result<SDL_AudioDeviceID>::Err(
            make_error_code( errors::AudioError::SubsystemNotInitialized )
        );
    }

    auto result = SDL_OpenAudioDevice( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr );
    if ( !result ) 
    {
        return Result<SDL_AudioDeviceID>::Err(
            make_error_code( errors::AudioError::DeviceOpenFailed ),
            SDL_GetError()
        );
    }

    return Result<SDL_AudioDeviceID>::Ok( result );
}
