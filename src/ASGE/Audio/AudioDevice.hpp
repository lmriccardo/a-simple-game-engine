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
 * @brief Owns SDL's audio subsystem, one playback device and a pool of AudioStreams.
 *
 * The pool is fixed at 32 streams. Initialize() brings up SDL_INIT_AUDIO and
 * opens the default playback device; CreateStream() then hands out a
 * shared_ptr<AudioStream> per clip being played. That pointer stays valid
 * even if DetachStream() relocates its slot. Shutdown() (also run from the
 * destructor) tears streams and device back down.
 */
class AudioDevice
{
private:
    static constexpr std::size_t kMaxNofStreams = 32; // Size of the stream pool
    std::array<std::shared_ptr<AudioStream>, kMaxNofStreams> m_Streams; // fixed pool of streams bound to m_DeviceId

    SDL_AudioDeviceID   m_DeviceId{0};           // the opened default-playback device, or 0 if not initialized
    bool                m_BackendInitialized{false}; // whether SDL_INIT_AUDIO is up
    std::size_t         m_LastIndex{0};          // one past the highest-ever-used slot in m_Streams
public:
    /**
     * @brief Shuts the device down.
     *
     * Calls Shutdown(), so it is safe whether or not Initialize() ran.
     */
    inline ~AudioDevice() { Shutdown(); }

    /**
     * @brief Initializes SDL's audio subsystem and opens the default playback device.
     *
     * The device is resumed and ready afterwards. On failure whatever was
     * already brought up is shut down again and the error is returned.
     */
    BoolResult Initialize();

    /**
     * @brief Closes the device and shuts down SDL's audio subsystem.
     *
     * Unbinds and resets every stream first. Each step only runs if its part
     * is up, so calling it twice, or before Initialize(), is harmless.
     */
    void Shutdown();

    /**
     * @brief Count of stream slots currently handed out.
     *
     * Grows on CreateStream() and shrinks on DetachStream().
     */
    [[nodiscard]] std::size_t Size() const noexcept;

    /**
     * @brief The id of the opened playback device.
     *
     * 0 if the device is not initialized.
     */
    [[nodiscard]] SDL_AudioDeviceID Id() const noexcept;

    /**
     * @brief Creates a new AudioStream matching inAudioClip's format and binds it to this device.
     *
     * Returns a shared handle into the stream pool, valid until
     * DetachStream() releases it. Fails if the device is not initialized,
     * the pool is full, or SDL cannot create or bind the stream.
     */
    [[nodiscard]] Result<std::shared_ptr<AudioStream>>
    CreateStream( media::AudioClip& inAudioClip ) noexcept;

    /**
     * @brief Unbinds and releases a stream previously returned by CreateStream().
     *
     * Swap-removes its slot: the trailing stream is relocated into it (and
     * told its new index) so the pool stays compact. Fails with InvalidStream
     * if inStream is already detached or belongs to another device.
     */
    [[nodiscard]] BoolResult DetachStream( AudioStream& inStream ) noexcept;

    /**
     * @brief Sets the master playback gain for every stream on this device.
     *
     * 1.0 leaves the volume unchanged and 0.0 is silent. Fails if the device
     * is not initialized or SDL rejects the gain.
     */
    BoolResult SetGain( float inVolume ) noexcept;
};

}