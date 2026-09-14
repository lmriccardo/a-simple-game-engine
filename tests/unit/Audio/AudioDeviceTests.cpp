#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Core/Errors.hpp>

#include <SDL3/SDL_hints.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

// AudioDevice::Initialize opens a real SDL_AudioDeviceID -- forcing SDL's
// "dummy" audio driver (same idea as SDLHeadlessFixture.hpp does for video)
// lets these tests exercise the real SDL_OpenAudioDevice/
// SDL_CreateAudioStream/SDL_BindAudioStream path safely on any CI runner,
// without ever touching a real audio device.
namespace
{

using namespace asge::audio;
using namespace asge::media;
using asge::errors::AudioError;

AudioClip MakeTestClip()
{
    return AudioClip(
        SDL_AudioSpec{ SDL_AUDIO_S16, 1, 8000 },
        AudioClip::data_t{ 0, 0, 0, 0 }
    );
}

class AudioDeviceTest : public ::testing::Test
{
protected:
    AudioDevice m_Device;

    void SetUp() override
    {
        SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );
    }
};

// ─── Initialize / Shutdown ──────────────────────────────────────────────────

TEST_F(AudioDeviceTest, Initialize_WithDummyDriverSucceeds)
{
    auto result = m_Device.Initialize();

    ASSERT_TRUE(result.IsOk());
    EXPECT_NE(m_Device.Id(), 0u);
    EXPECT_EQ(m_Device.Size(), 0u);
}

TEST_F(AudioDeviceTest, Shutdown_ResetsIdAndStreamCount)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clip = MakeTestClip();
    ASSERT_TRUE(m_Device.CreateStream(clip).IsOk());

    m_Device.Shutdown();

    EXPECT_EQ(m_Device.Id(), 0u);
    EXPECT_EQ(m_Device.Size(), 0u);
}

TEST_F(AudioDeviceTest, Shutdown_DeviceCanBeReinitializedAfterwards)
{
    // Regression test: Shutdown() used to leave m_LastIndex non-zero (the
    // stream array was never actually cleared -- see the next test's
    // comment), so a device reused after Shutdown() would see stale state.
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    m_Device.Shutdown();

    auto result = m_Device.Initialize();

    EXPECT_TRUE(result.IsOk());
    EXPECT_EQ(m_Device.Size(), 0u);
}

// ─── CreateStream ───────────────────────────────────────────────────────────

TEST_F(AudioDeviceTest, CreateStream_BeforeInitializeReturnsSubsystemNotInitializedError)
{
    auto clip = MakeTestClip();

    auto result = m_Device.CreateStream(clip);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AudioError::SubsystemNotInitialized));
}

TEST_F(AudioDeviceTest, CreateStream_AfterInitializeReturnsValidStreamAndGrowsSize)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clip = MakeTestClip();

    auto result = m_Device.CreateStream(clip);

    ASSERT_TRUE(result.IsOk());
    auto* stream = result.Value();
    ASSERT_NE(stream, nullptr);
    EXPECT_TRUE(stream->IsValid());
    EXPECT_EQ(stream->Index(), 0u);
    EXPECT_EQ(m_Device.Size(), 1u);
}

TEST_F(AudioDeviceTest, CreateStream_MultipleCallsReturnDistinctSlots)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clipA = MakeTestClip();
    auto clipB = MakeTestClip();

    auto* streamA = m_Device.CreateStream(clipA).Value();
    auto* streamB = m_Device.CreateStream(clipB).Value();

    EXPECT_NE(streamA, streamB);
    EXPECT_EQ(streamA->Index(), 0u);
    EXPECT_EQ(streamB->Index(), 1u);
    EXPECT_EQ(m_Device.Size(), 2u);
}

TEST_F(AudioDeviceTest, CreateStream_ExhaustingThePoolReturnsStreamCreationFailedError)
{
    // Regression test: filling every one of the pool's 32 slots used to
    // overrun m_Streams.at(m_LastIndex) inside CreateStream -- a noexcept
    // function -- throwing std::out_of_range and crashing the whole process
    // via std::terminate instead of returning this error.
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clip = MakeTestClip();

    for ( int i = 0; i < 32; ++i )
    {
        ASSERT_TRUE(m_Device.CreateStream(clip).IsOk()) << "slot " << i;
    }

    auto result = m_Device.CreateStream(clip);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AudioError::StreamCreationFailed));
    EXPECT_EQ(m_Device.Size(), 32u);
}

// ─── DetachStream ───────────────────────────────────────────────────────────

TEST_F(AudioDeviceTest, DetachStream_NeverAttachedStreamReturnsInvalidStreamError)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    AudioStream neverCreated;

    auto result = m_Device.DetachStream(neverCreated);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AudioError::InvalidStream));
}

TEST_F(AudioDeviceTest, DetachStream_TrailingSlotShrinksSize)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clip = MakeTestClip();
    auto* stream = m_Device.CreateStream(clip).Value();

    auto result = m_Device.DetachStream(*stream);

    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(stream->IsValid());
    EXPECT_EQ(m_Device.Size(), 0u);
}

TEST_F(AudioDeviceTest, DetachStream_AlreadyDetachedReturnsInvalidStreamError)
{
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clip = MakeTestClip();
    auto* stream = m_Device.CreateStream(clip).Value();
    ASSERT_TRUE(m_Device.DetachStream(*stream).IsOk());

    auto result = m_Device.DetachStream(*stream);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AudioError::InvalidStream));
}

TEST_F(AudioDeviceTest, DetachStream_MiddleSlotDoesNotDisturbOtherLiveStreams)
{
    // Regression test: detaching a non-trailing slot used to swap the
    // trailing stream into its place -- silently repointing whoever held
    // *that* stream's AudioStream* at a slot that no longer held their
    // stream, and marking their own now-relocated stream invalid, purely as
    // a side effect of some other, unrelated source being detached.
    ASSERT_TRUE(m_Device.Initialize().IsOk());
    auto clipA = MakeTestClip();
    auto clipB = MakeTestClip();
    auto clipC = MakeTestClip();

    auto* streamA = m_Device.CreateStream(clipA).Value();
    auto* streamB = m_Device.CreateStream(clipB).Value();
    auto* streamC = m_Device.CreateStream(clipC).Value();
    std::size_t const cIndexBefore = streamC->Index();

    auto result = m_Device.DetachStream(*streamB);

    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(streamA->IsValid());
    EXPECT_EQ(streamA->Index(), 0u);
    EXPECT_TRUE(streamC->IsValid());
    EXPECT_EQ(streamC->Index(), cIndexBefore);
}

}
