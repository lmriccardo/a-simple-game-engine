#pragma once

#include <cstdint>
#include <memory>
#include <SDL3/SDL_audio.h>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>

namespace asge::audio
{

class AudioDevice
{
public:
    using stream     = std::shared_ptr<SDL_AudioStream>;
    using stream_tag = std::weak_ptr<SDL_AudioStream>;

private:
    static constexpr std::size_t kMaxNofStreams = 32;
    std::array<stream, kMaxNofStreams> m_Streams;

    SDL_AudioDeviceID   m_DeviceId{0};
    bool                m_BackendInitialized{false};
    std::size_t         m_LastIndex{0};
public:
    inline ~AudioDevice() { Shutdown(); }

    BoolResult Initialize();
    void Shutdown();
    
    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] SDL_AudioDeviceID Id() const noexcept;
    [[nodiscard]] Result<stream_tag> 
    CreateStream( media::AudioClip& inAudioClip ) noexcept;
};

}