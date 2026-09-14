#include "AudioStream.hpp"

SDL_AudioStream *asge::audio::AudioStream::Get() const noexcept
{
    return m_Stream.get();
}

bool asge::audio::AudioStream::IsValid() const noexcept
{
    return Get() != nullptr;
}

std::size_t asge::audio::AudioStream::Index() const noexcept
{
    return m_InDevIdx;
}

void asge::audio::AudioStream::Index(std::size_t inIndex) noexcept
{
    m_InDevIdx = inIndex;
}

void asge::audio::AudioStream::Reset() noexcept
{
    m_Stream.reset();
}

asge::BoolResult asge::audio::AudioStream::ClearData() noexcept
{
    if ( !IsValid() )
    {
        return BoolResult::Err( make_error_code( errors::AudioError::InvalidStream ) );
    }

    if ( SDL_ClearAudioStream( Get() ) )
    {
        return BoolResult::Ok();
    }

    return BoolResult::Err(
        make_error_code( errors::AudioError::InvalidStream ), SDL_GetError()
    );
}

asge::BoolResult asge::audio::AudioStream::PutData(media::AudioClip &inClip) noexcept
{
    if ( !SDL_PutAudioStreamData( Get(), inClip.Data(), static_cast<int>(inClip.Size()) ) )
    {
        return BoolResult::Err(
            make_error_code( errors::AudioError::InvalidStream ), SDL_GetError()
        );
    }

    if ( !SDL_FlushAudioStream( Get() ) )
    {
        return BoolResult::Err(
            make_error_code( errors::AudioError::InvalidStream ), SDL_GetError()
        );
    }

    return BoolResult::Ok();
}

bool asge::audio::AudioStream::IsDataAvailable() const noexcept
{
    if ( !IsValid() ) return false;
    return SDL_GetAudioStreamAvailable( Get() ) != 0;
}
