#include <ASGE/Audio/AudioStream.hpp>
#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Core/Errors.hpp>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>

#include <gtest/gtest.h>

#include <cstdint>

// AudioStream is a thin handle AudioDevice::CreateStream hands out -- a real,
// bound SDL_AudioStream is needed to exercise ClearData/PutData/
// IsDataAvailable, so these go through a real AudioDevice on SDL's "dummy"
// audio driver (see AudioDeviceTests.cpp for why that's safe in CI).
namespace
{

using namespace asge::audio;
using namespace asge::media;
using asge::errors::AudioError;

AudioClip MakeTestClip()
{
    return AudioClip(
        SDL_AudioSpec{ SDL_AUDIO_S16, 1, 8000 },
        AudioClip::data_t{ 1, 2, 3, 4 }
    );
}

// ─── Default state ──────────────────────────────────────────────────────────

TEST(AudioStreamTest, DefaultConstructed_IsInvalidWithNoIndex)
{
    AudioStream stream;

    EXPECT_EQ(stream.Get(), nullptr);
    EXPECT_FALSE(stream.IsValid());
    EXPECT_EQ(stream.Index(), static_cast<std::size_t>(-1));
}

TEST(AudioStreamTest, Index_SetterRoundTrips)
{
    AudioStream stream;

    stream.Index(7);

    EXPECT_EQ(stream.Index(), 7u);
}

TEST(AudioStreamTest, Reset_OnDefaultConstructed_StaysInvalid)
{
    AudioStream stream;

    stream.Reset();

    EXPECT_EQ(stream.Get(), nullptr);
    EXPECT_FALSE(stream.IsValid());
}

// ─── ClearData / PutData / IsDataAvailable (invalid stream) ─────────────────

TEST(AudioStreamTest, ClearData_OnInvalidStreamReturnsInvalidStreamError)
{
    AudioStream stream;

    auto result = stream.ClearData();

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AudioError::InvalidStream));
}

TEST(AudioStreamTest, IsDataAvailable_OnInvalidStreamIsFalse)
{
    AudioStream stream;

    EXPECT_FALSE(stream.IsDataAvailable());
}

// ─── ClearData / PutData / IsDataAvailable (real, bound stream) ─────────────

class BoundAudioStreamTest : public ::testing::Test
{
protected:
    AudioDevice m_Device;

    void SetUp() override
    {
        SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );
        ASSERT_TRUE(m_Device.Initialize().IsOk());
        // Deterministic: nothing but this test's own calls should ever
        // touch a stream's queued data.
        SDL_PauseAudioDevice( m_Device.Id() );
    }
};

TEST_F(BoundAudioStreamTest, PutData_QueuesTheClipsWholeBuffer)
{
    auto clip = MakeTestClip();
    auto stream = m_Device.CreateStream(clip).Value();

    auto result = stream->PutData(clip);

    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(stream->IsDataAvailable());
    EXPECT_EQ(SDL_GetAudioStreamQueued(stream->Get()), static_cast<int>(clip.Size()));
}

TEST_F(BoundAudioStreamTest, ClearData_OnValidStreamEmptiesTheQueue)
{
    auto clip = MakeTestClip();
    auto stream = m_Device.CreateStream(clip).Value();
    ASSERT_TRUE(stream->PutData(clip).IsOk());
    ASSERT_TRUE(stream->IsDataAvailable());

    auto result = stream->ClearData();

    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(stream->IsDataAvailable());
}

}
