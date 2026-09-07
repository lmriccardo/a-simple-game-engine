#pragma once

#include <SDL3/SDL_audio.h>
#include <ASGE/Core/Errors.hpp>

namespace asge::audio
{

/**
 * @brief Owns SDL's audio subsystem and a single default-playback stream.
 *
 * Mirrors VideoSystem's shape: Initialize() brings up SDL_INIT_AUDIO and
 * opens a stereo 48kHz float stream on the default playback device,
 * Shutdown() (also run from the destructor) tears both back down. Stream()
 * exposes the raw SDL_AudioStream for higher-level playback code to feed.
 */
class AudioDevice
{
private:
    SDL_AudioStream* m_Stream{nullptr};          // The opened default-playback audio stream
    bool m_BackendInitialized{false};            // Whether SDL_INIT_AUDIO is up
public:
    inline ~AudioDevice() { Shutdown(); }

    /**
     * @brief Initializes the SDL audio subsystem and opens a default
     *        playback stream (stereo, 48kHz, float samples), resumed
     *        and ready to receive audio immediately.
     */
    BoolResult Initialize();

    // Destroys the audio stream and shuts down the SDL audio subsystem, if either is up
    void Shutdown();

    // Returns the underlying default-playback audio stream, or nullptr if not initialized
    [[nodiscard]] SDL_AudioStream* Stream() const noexcept;
};

}