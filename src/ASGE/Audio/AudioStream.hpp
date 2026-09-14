#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>

#include <SDL3/SDL_audio.h>

namespace asge::audio
{

/**
 * @brief A shared handle to one SDL_AudioStream bound to an AudioDevice,
 *        plus the slot it occupies in that device's stream pool.
 *
 * Copies share the same underlying SDL_AudioStream (via shared_ptr) and the
 * same Index() -- AudioDevice::CreateStream is the only place a fresh one
 * is minted. Not meant to be constructed directly by callers other than
 * AudioDevice; see its doc comment for how the pool owns these slots.
 */
class AudioStream
{
    std::shared_ptr<SDL_AudioStream> m_Stream;    // The actual SDL Stream with data
    std::size_t                      m_InDevIdx;  // The stream position into the dev array
public:
    AudioStream() : AudioStream(nullptr, static_cast<std::size_t>(-1)) {}
    AudioStream( std::shared_ptr<SDL_AudioStream> inStream, std::size_t inIdx )
        : m_Stream( std::move(inStream) ), m_InDevIdx( inIdx )
    {}

    AudioStream( AudioStream const& ) = default;
    AudioStream( AudioStream && ) = default;
    AudioStream& operator=( AudioStream const& ) = default;
    AudioStream& operator=( AudioStream && ) = default;

    /** @brief The underlying SDL stream handle, or nullptr once Reset(). */
    [[nodiscard]] SDL_AudioStream* Get() const noexcept;

    /** @brief True if this still wraps a live SDL stream (i.e. Get() isn't null). */
    [[nodiscard]] bool IsValid() const noexcept;

    /** @brief This stream's slot index in its owning AudioDevice's pool, or -1 once detached. */
    [[nodiscard]] std::size_t Index() const noexcept;

    // Sets this stream's slot index -- for AudioDevice's own bookkeeping, not callers
    void Index( std::size_t inIndex ) noexcept;

    // Releases the underlying SDL stream handle, leaving this instance invalid
    void Reset() noexcept;

    /** @brief Discards any PCM data currently queued on this stream. */
    BoolResult ClearData() noexcept;

    /** @brief Queues inClip's whole PCM buffer onto this stream and flushes it for playback. */
    BoolResult PutData( media::AudioClip& inClip ) noexcept;

    /** @brief True if this stream still has queued PCM data the device hasn't consumed yet. */
    bool IsDataAvailable() const noexcept;
};

}