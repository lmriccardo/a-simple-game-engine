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
    AudioClip() = delete;

    /** @brief Wraps already-decoded PCM samples and their format; prefer Load() to decode from a file. */
    AudioClip( SDL_AudioSpec inSpec, data_t inData )
        : m_Spec( inSpec ), m_Data( std::move(inData) )
    {}

    AudioClip( AudioClip const& ) = delete;
    AudioClip& operator=( AudioClip const& ) = delete;
    AudioClip( AudioClip&& ) = default;
    AudioClip& operator=( AudioClip&& ) = default;

    /** @brief Sample format, channel count and frequency of the decoded audio. */
    [[nodiscard]] SDL_AudioSpec const& Spec() const noexcept;

    /** @brief Pointer to the decoded raw PCM sample bytes. */
    [[nodiscard]] std::uint8_t const* Data() const noexcept;

    /** @brief Size, in bytes, of the buffer returned by Data(). */
    [[nodiscard]] std::size_t Size() const noexcept;

    /**
     * @brief Reads an audio file from disk and decodes it (WAV or OGG,
     * dispatched by extension) into raw PCM.
     */
    [[nodiscard]] static Result<AudioClip> Load( filesystem::Path const& inPath ) noexcept;
};

[[nodiscard]] bool IsAudioSystemInitialized() noexcept;
[[nodiscard]] BoolResult InitializeAudioSystem() noexcept;
[[nodiscard]] Result<SDL_AudioDeviceID> OpenNewAudioDevice() noexcept;

}