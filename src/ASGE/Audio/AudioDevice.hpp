#pragma once

#include <cstdint>
#include <memory>
#include <array>
#include <SDL3/SDL_audio.h>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>

#include "AudioStream.hpp"

namespace asge::audio
{

/**
 * @brief Owns SDL's audio subsystem, one physical playback device, and a
 *        fixed pool of up to 32 AudioStreams bound to it.
 *
 * Initialize() brings up SDL_INIT_AUDIO and opens the default playback
 * device; CreateStream() then hands out a shared_ptr<AudioStream> per
 * AudioSource wanting to play a clip, backed by a slot in the fixed
 * m_Streams array. Callers keep that shared_ptr for as long as they're
 * playing (see AudioSystem) — it stays valid regardless of which slot
 * backs it, even across a DetachStream() call elsewhere that swap-removes
 * and relocates a slot still in use. Shutdown() (also run from the
 * destructor) tears both the streams and the device back down.
 */
class AudioDevice
{
private:
    static constexpr std::size_t kMaxNofStreams = 32;
    std::array<std::shared_ptr<AudioStream>, kMaxNofStreams> m_Streams; // fixed pool of streams bound to m_DeviceId

    SDL_AudioDeviceID   m_DeviceId{0};           // the opened default-playback device, or 0 if not initialized
    bool                m_BackendInitialized{false}; // whether SDL_INIT_AUDIO is up
    std::size_t         m_LastIndex{0};          // one past the highest-ever-used slot in m_Streams
public:
    inline ~AudioDevice() { Shutdown(); }

    /** @brief Initializes SDL's audio subsystem and opens the default playback device, resumed and ready. */
    BoolResult Initialize();

    // Unbinds and resets every stream, closes the device and shuts down the SDL audio subsystem, if either is up
    void Shutdown();

    /** @brief Count of stream slots currently handed out (grows on CreateStream, shrinks on DetachStream). */
    [[nodiscard]] std::size_t Size() const noexcept;

    // Returns the id of the opened playback device, or 0 if not initialized
    [[nodiscard]] SDL_AudioDeviceID Id() const noexcept;

    /**
     * @brief Creates a new AudioStream matching inAudioClip's format, binds
     *        it to this device, and returns a shared handle into this
     *        device's stream pool that stays valid until DetachStream()
     *        releases it.
     */
    [[nodiscard]] Result<std::shared_ptr<AudioStream>>
    CreateStream( media::AudioClip& inAudioClip ) noexcept;

    /**
     * @brief Unbinds and releases a stream previously returned by
     *        CreateStream(), swap-removing its slot (relocating the
     *        trailing stream into it, if it wasn't already the trailing
     *        one) so the pool stays compact.
     */
    [[nodiscard]] BoolResult DetachStream( AudioStream& inStream ) noexcept;

    /** @brief Sets the master playback gain for every stream on this device (1.0 = unchanged, 0.0 = silent). */
    BoolResult SetGain( float inVolume ) noexcept;
};

}