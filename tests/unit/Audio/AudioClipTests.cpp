#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// AudioClip::Load's WAV path is exercised with a hand-built RIFF/WAVE fixture
// (same approach as the BMP bytes in ImageTests.cpp); Ogg Vorbis's
// codebook/packet framing isn't practical to hand-construct, so its path is
// exercised against a small real fixture instead -- tests/support/audio/
// tone.ogg, see NOTICE.md there for how it was generated.
namespace
{

using namespace asge::media;
using asge::errors::AudioError;

void PushU16( std::vector<std::byte>& outBytes, std::uint16_t inValue )
{
    outBytes.push_back( static_cast<std::byte>( inValue & 0xFF ) );
    outBytes.push_back( static_cast<std::byte>( (inValue >> 8) & 0xFF ) );
}

void PushU32( std::vector<std::byte>& outBytes, std::uint32_t inValue )
{
    outBytes.push_back( static_cast<std::byte>( inValue & 0xFF ) );
    outBytes.push_back( static_cast<std::byte>( (inValue >> 8) & 0xFF ) );
    outBytes.push_back( static_cast<std::byte>( (inValue >> 16) & 0xFF ) );
    outBytes.push_back( static_cast<std::byte>( (inValue >> 24) & 0xFF ) );
}

void PushTag( std::vector<std::byte>& outBytes, char const (&inTag)[5] )
{
    for ( int i = 0; i < 4; ++i ) outBytes.push_back( static_cast<std::byte>(inTag[i]) );
}

// A minimal 16-bit PCM mono RIFF/WAVE file -- 4 arbitrary samples.
std::vector<std::byte> MakeMinimalWav()
{
    constexpr std::uint32_t kSampleRate = 8000;
    constexpr std::uint16_t kChannels = 1;
    constexpr std::uint16_t kBitsPerSample = 16;
    constexpr std::int16_t kSamples[] = { 0, 1000, -1000, 0 };
    constexpr std::uint32_t kDataSize = sizeof(kSamples);
    constexpr std::uint16_t kBlockAlign = kChannels * kBitsPerSample / 8;
    constexpr std::uint32_t kByteRate = kSampleRate * kBlockAlign;

    std::vector<std::byte> wav;

    PushTag( wav, "RIFF" );
    PushU32( wav, 36 + kDataSize );
    PushTag( wav, "WAVE" );

    PushTag( wav, "fmt " );
    PushU32( wav, 16 ); // fmt chunk size
    PushU16( wav, 1 );  // PCM
    PushU16( wav, kChannels );
    PushU32( wav, kSampleRate );
    PushU32( wav, kByteRate );
    PushU16( wav, kBlockAlign );
    PushU16( wav, kBitsPerSample );

    PushTag( wav, "data" );
    PushU32( wav, kDataSize );
    for ( auto sample : kSamples ) PushU16( wav, static_cast<std::uint16_t>(sample) );

    return wav;
}

std::vector<std::byte> MakeGarbageBytes()
{
    return { std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03} };
}

std::filesystem::path TonePath()
{
    return std::filesystem::path(ASGE_TEST_AUDIO_DIR) / "tone.ogg";
}

class AudioClipLoadTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Path;

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove( m_Path, ec );
    }

    [[nodiscard]] std::filesystem::path MakeTempPath( std::string const& inExtension ) const
    {
        auto const uniqueName = "asge_audioclip_test_"
            + std::to_string( reinterpret_cast<std::uintptr_t>(this) ) + inExtension;
        return std::filesystem::temp_directory_path() / uniqueName;
    }

    void WriteFile( std::filesystem::path const& inPath, std::vector<std::byte> const& inBytes ) const
    {
        std::ofstream file( inPath, std::ios::binary | std::ios::trunc );
        file.write( reinterpret_cast<char const*>(inBytes.data()), static_cast<std::streamsize>(inBytes.size()) );
    }
};

TEST_F(AudioClipLoadTest, Load_NonExistentPathReturnsError)
{
    m_Path = MakeTempPath( ".wav" );

    auto result = AudioClip::Load( m_Path );

    EXPECT_FALSE( result.IsOk() );
}

TEST_F(AudioClipLoadTest, Load_UnsupportedExtensionReturnsInvalidFormatError)
{
    m_Path = MakeTempPath( ".mp3" );
    WriteFile( m_Path, MakeGarbageBytes() );

    auto result = AudioClip::Load( m_Path );

    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), make_error_code(AudioError::InvalidFormat) );
}

TEST_F(AudioClipLoadTest, Load_WavGarbageBytesReturnsDecodeFailedError)
{
    m_Path = MakeTempPath( ".wav" );
    WriteFile( m_Path, MakeGarbageBytes() );

    auto result = AudioClip::Load( m_Path );

    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), make_error_code(AudioError::DecodeFailed) );
}

TEST_F(AudioClipLoadTest, Load_OggGarbageBytesReturnsDecodeFailedError)
{
    m_Path = MakeTempPath( ".ogg" );
    WriteFile( m_Path, MakeGarbageBytes() );

    auto result = AudioClip::Load( m_Path );

    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), make_error_code(AudioError::DecodeFailed) );
}

TEST_F(AudioClipLoadTest, Load_ValidWavFileDecodesSuccessfully)
{
    m_Path = MakeTempPath( ".wav" );
    WriteFile( m_Path, MakeMinimalWav() );

    auto result = AudioClip::Load( m_Path );

    ASSERT_TRUE( result.IsOk() );
    auto const& clip = result.Value();
    EXPECT_EQ( clip.Spec().freq, 8000 );
    EXPECT_EQ( clip.Spec().channels, 1 );
    EXPECT_GT( clip.Size(), 0u );
    ASSERT_NE( clip.Data(), nullptr );
}

// ─── Ogg fixture ────────────────────────────────────────────────────────

TEST(AudioClipLoadOggFixtureTest, Load_ValidOggFileDecodesSuccessfully)
{
    auto result = AudioClip::Load( TonePath() );

    ASSERT_TRUE( result.IsOk() );
    auto const& clip = result.Value();
    EXPECT_EQ( clip.Spec().channels, 1 );
    EXPECT_EQ( clip.Spec().freq, 8000 );
    EXPECT_GT( clip.Size(), 0u );
    ASSERT_NE( clip.Data(), nullptr );
}

}
