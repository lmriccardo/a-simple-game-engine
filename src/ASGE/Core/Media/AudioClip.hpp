#pragma once

#include <cstdint>
#include <vector>
#include <SDL3/SDL_audio.h>
#include <ASGE/Core/Filesystem/FileData.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::media
{

/**
 * @brief Decoded PCM audio samples plus the SDL_AudioSpec describing them.
 *
 * Produced by decoding a WAV or Ogg Vorbis file (see Load) into raw PCM
 * ready for SDL's audio pipeline. Move-only, since the decoded sample
 * buffer can be sizeable and copying it is rarely wanted.
 */
class AudioClip
{
public:
    using data_t = std::vector<std::uint8_t>;
private:
    SDL_AudioSpec m_Spec; // sample format, channel count and frequency of m_Data
    data_t        m_Data; // decoded raw PCM samples
public:
    /**
     * @brief Default construction is disabled.
     */
    AudioClip() = delete;

    /**
     * @brief Wraps already-decoded PCM samples and their format.
     *
     * Prefer Load() to decode from a file. inData is moved in, not copied.
     */
    AudioClip( SDL_AudioSpec inSpec, data_t inData )
        : m_Spec( inSpec ), m_Data( std::move(inData) )
    {}

    /**
     * @brief Copy construction is disabled.
     */
    AudioClip( AudioClip const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    AudioClip& operator=( AudioClip const& ) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    AudioClip( AudioClip&& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    AudioClip& operator=( AudioClip&& ) = default;

    /**
     * @brief Sample format, channel count and frequency of the decoded audio.
     */
    [[nodiscard]] SDL_AudioSpec const& Spec() const noexcept;

    /**
     * @brief Pointer to the decoded raw PCM sample bytes.
     *
     * Valid for the clip's lifetime.
     */
    [[nodiscard]] std::uint8_t const* Data() const noexcept;

    /**
     * @brief Size, in bytes, of the buffer returned by Data().
     */
    [[nodiscard]] std::size_t Size() const noexcept;

    /**
     * @brief Reads an audio file from disk and decodes it into raw PCM.
     *
     * The decoder is chosen by the lowercase extension: `.wav` goes through
     * SDL (format kept as in the file), `.ogg` through stb_vorbis (always
     * signed 16-bit). Any other extension, including an upper-case one, fails
     * with AudioError::InvalidFormat. A file-read error is returned unchanged.
     */
    [[nodiscard]] static Result<AudioClip> Load( filesystem::Path const& inPath ) noexcept;
};

/**
 * @brief True if SDL's audio subsystem (SDL_INIT_AUDIO) is already up.
 */
[[nodiscard]] bool IsAudioSystemInitialized() noexcept;

/**
 * @brief Initializes SDL's audio subsystem; a no-op success if already up.
 *
 * Fails with AudioError::SubsystemInitFailed, carrying SDL's message, if
 * SDL_Init cannot start it.
 */
[[nodiscard]] BoolResult InitializeAudioSystem() noexcept;

/**
 * @brief Opens the default playback device.
 *
 * Fails with AudioError::SubsystemNotInitialized if the audio subsystem (see
 * InitializeAudioSystem) isn't up yet, or AudioError::DeviceOpenFailed if SDL
 * cannot open a device. The caller owns the returned device id.
 */
[[nodiscard]] Result<SDL_AudioDeviceID> OpenNewAudioDevice() noexcept;

}