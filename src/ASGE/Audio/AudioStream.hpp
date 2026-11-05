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
 * @brief A shared handle to one SDL_AudioStream bound to an AudioDevice.
 *
 * Also records the slot it occupies in that device's stream pool. Copies
 * share the same underlying SDL_AudioStream (via shared_ptr) and the same
 * Index(). AudioDevice::CreateStream is the only place a fresh one is made;
 * callers should not construct one directly.
 */
class AudioStream
{
    std::shared_ptr<SDL_AudioStream> m_Stream;    // The actual SDL Stream with data
    std::size_t                      m_InDevIdx;  // The stream position into the dev array
public:
    /**
     * @brief Creates an invalid stream, with no SDL stream and index -1.
     */
    AudioStream() : AudioStream(nullptr, static_cast<std::size_t>(-1)) {}

    /**
     * @brief Wraps inStream, recording inIdx as its pool slot.
     *
     * For AudioDevice's own use.
     */
    AudioStream( std::shared_ptr<SDL_AudioStream> inStream, std::size_t inIdx )
        : m_Stream( std::move(inStream) ), m_InDevIdx( inIdx )
    {}

    /**
     * @brief Copy constructor.
     *
     * Defaulted; copies each member.
     */
    AudioStream( AudioStream const& ) = default;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    AudioStream( AudioStream && ) = default;

    /**
     * @brief Copy assignment.
     *
     * Defaulted; assigns each member.
     */
    AudioStream& operator=( AudioStream const& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    AudioStream& operator=( AudioStream && ) = default;

    /**
     * @brief The underlying SDL stream handle.
     *
     * nullptr once Reset().
     */
    [[nodiscard]] SDL_AudioStream* Get() const noexcept;

    /**
     * @brief True if this still wraps a live SDL stream.
     *
     * That is, Get() isn't null.
     */
    [[nodiscard]] bool IsValid() const noexcept;

    /**
     * @brief This stream's slot index in its owning AudioDevice's pool.
     *
     * -1 once detached.
     */
    [[nodiscard]] std::size_t Index() const noexcept;

    /**
     * @brief Sets this stream's slot index.
     *
     * For AudioDevice's own bookkeeping, not callers.
     */
    void Index( std::size_t inIndex ) noexcept;

    /**
     * @brief Releases the underlying SDL stream handle.
     *
     * Leaves this instance invalid; copies sharing it keep it alive until
     * they are reset too.
     */
    void Reset() noexcept;

    /**
     * @brief Discards any PCM data currently queued on this stream.
     *
     * Fails with InvalidStream if the stream is invalid or SDL rejects it.
     */
    BoolResult ClearData() noexcept;

    /**
     * @brief Queues inClip's whole PCM buffer onto this stream and flushes it.
     *
     * Playback starts once the device consumes the data. Fails with
     * InvalidStream if the stream is invalid or SDL rejects the data.
     */
    BoolResult PutData( media::AudioClip& inClip ) noexcept;

    /**
     * @brief True if the device hasn't consumed all queued PCM data yet.
     *
     * False for an invalid stream.
     */
    bool IsDataAvailable() const noexcept;

    /**
     * @brief Sets this stream's own playback gain.
     *
     * 1.0 leaves the volume unchanged and 0.0 is silent. Independent of the
     * device's master gain. Fails with InvalidStream on an invalid stream.
     */
    BoolResult SetAudioGain( float inVolume ) noexcept;
};

}