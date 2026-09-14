#include <ASGE/Game/Systems/AudioSystem.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>

#include <gtest/gtest.h>

#include <cstdint>

// AudioSystem is exercised against a real AudioDevice bound to SDL's "dummy"
// audio driver (see AudioDeviceTests.cpp). The device is paused right after
// Initialize() so nothing but these tests' own synchronous SDL calls ever
// pulls from a stream's queue -- byte counts stay fully deterministic
// instead of racing a background mixer thread.
namespace
{

using namespace asge::game;
using namespace asge::game::components;
using namespace asge::audio;
using namespace asge::media;
using namespace asge::ecs;

AudioClip MakeClip( std::size_t inByteCount )
{
    return AudioClip(
        SDL_AudioSpec{ SDL_AUDIO_S16, 1, 8000 },
        AudioClip::data_t( inByteCount, std::uint8_t{0} )
    );
}

AudioSource::audio_clip_asset MakeClipAsset( std::size_t inByteCount )
{
    return asset::Asset<AudioClip>::Create( "audio/test.wav", MakeClip(inByteCount) );
}

class AudioSystemTest : public ::testing::Test
{
protected:
    Registry    m_Registry;
    AudioDevice m_Device;
    Entity      m_Entity{ Entity::Null() };

    void SetUp() override
    {
        SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );
        ASSERT_TRUE(m_Device.Initialize().IsOk());
        SDL_PauseAudioDevice( m_Device.Id() ); // deterministic: only this test drains streams

        auto created = m_Registry.CreateEntity();
        ASSERT_TRUE(created.IsOk());
        m_Entity = created.Value();
        ASSERT_TRUE(m_Registry.AddComponent<AudioSource>( m_Entity, AudioSource{} ).IsOk());
    }

    AudioSource& Source()
    {
        return m_Registry.GetComponent<AudioSource>( m_Entity ).Value().get();
    }

    void Tick() { systems::AudioSystem( m_Registry, m_Device ); }
};

// ─── Starting playback ──────────────────────────────────────────────────────

TEST_F(AudioSystemTest, Tick_PlayingWithNoClipYetDoesNothing)
{
    Source().m_Playing = true; // e.g. PlayAudioSource() called before the clip resolved

    Tick();

    EXPECT_EQ(Source().m_Stream, nullptr);
}

TEST_F(AudioSystemTest, Tick_NotPlayingNeverCreatesAStream)
{
    Source().m_Clip = MakeClipAsset(64);

    Tick();

    EXPECT_EQ(Source().m_Stream, nullptr);
}

TEST_F(AudioSystemTest, Tick_FirstTickOfPlayingSourceCreatesAndFillsItsStream)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );

    Tick();

    ASSERT_NE(Source().m_Stream, nullptr);
    EXPECT_TRUE(Source().m_Stream->IsValid());
    EXPECT_TRUE(Source().m_Stream->IsDataAvailable());
    EXPECT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);
}

// ─── The restart-every-tick regression ──────────────────────────────────────

TEST_F(AudioSystemTest, Tick_StillPlayingWithDataLeftDoesNotRestartTheClip)
{
    // Regression test: AudioSystem used to call StartOrReplaySource (clear +
    // re-push the whole clip) on *every* tick a source was playing, not just
    // the one that started it -- so a source could never play past its very
    // first buffer's worth of audio, restarting from byte 0 every tick.
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick(); // starts it -- 64 bytes now queued
    ASSERT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);

    // Simulate some of the clip having already played, deterministically
    // (no waiting on a device thread): pull a few bytes out ourselves. SDL
    // is free to hand back fewer bytes than requested (format conversion
    // may not produce an exact match), so only assert *some* were consumed.
    std::uint8_t scratch[16];
    int const gotBytes = SDL_GetAudioStreamData(Source().m_Stream->Get(), scratch, sizeof(scratch));
    ASSERT_GT(gotBytes, 0);
    int const queuedAfterPartialPlay = SDL_GetAudioStreamQueued(Source().m_Stream->Get());
    ASSERT_LT(queuedAfterPartialPlay, 64);

    Tick(); // still playing, clip still has data left -- must be a no-op

    EXPECT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), queuedAfterPartialPlay);
}

TEST_F(AudioSystemTest, Tick_SubsequentTicksReuseTheSameStreamInstance)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick();
    auto firstStream = Source().m_Stream;

    Tick();
    Tick();

    EXPECT_EQ(Source().m_Stream, firstStream);
    EXPECT_EQ(m_Device.Size(), 1u); // never created a second stream for the same source
}

// ─── Finishing / looping ────────────────────────────────────────────────────

TEST_F(AudioSystemTest, Tick_NonLoopingSourceThatRanOutOfDataStops)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source(), /*inLoop=*/false );
    Tick();
    ASSERT_TRUE(Source().m_Stream->IsDataAvailable());

    Source().m_Stream->ClearData(); // simulate having fully played out
    Tick();

    EXPECT_FALSE(Source().m_Playing);
}

TEST_F(AudioSystemTest, Tick_LoopingSourceThatRanOutOfDataRestarts)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source(), /*inLoop=*/true );
    Tick();

    Source().m_Stream->ClearData(); // simulate having fully played out
    Tick();

    EXPECT_TRUE(Source().m_Playing);
    EXPECT_TRUE(Source().m_Stream->IsDataAvailable());
    EXPECT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);
}

// ─── Replaying via PlayAudioSource (m_Restart) ─────────────────────────────

TEST_F(AudioSystemTest, Tick_ReplayingANonLoopingStoppedSourceRestartsImmediately)
{
    // Regression test: without an explicit restart signal, replaying a
    // non-looping source that had already stopped was indistinguishable
    // from "still finishing naturally" -- the stream existed and had no
    // data available either way -- so the tick right after PlayAudioSource
    // would immediately re-stop it (m_Loop is false) instead of ever
    // restarting, silently dropping the replay request.
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source(), /*inLoop=*/false );
    Tick();
    Source().m_Stream->ClearData(); // ran out naturally
    Tick();
    ASSERT_FALSE(Source().m_Playing);
    auto originalStream = Source().m_Stream;

    PlayAudioSource( Source(), /*inLoop=*/false );
    Tick();

    EXPECT_TRUE(Source().m_Playing);
    EXPECT_TRUE(Source().m_Stream->IsDataAvailable());
    EXPECT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);
    EXPECT_EQ(Source().m_Stream, originalStream); // reused, not recreated
    EXPECT_EQ(m_Device.Size(), 1u);
}

TEST_F(AudioSystemTest, Tick_ReplayingAStillPlayingSourceRestartsFromTheBeginning)
{
    // PlayAudioSource is a "restart now" request even mid-playthrough, not
    // just once the previous playthrough has finished.
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick();
    std::uint8_t scratch[16];
    SDL_GetAudioStreamData(Source().m_Stream->Get(), scratch, sizeof(scratch));
    ASSERT_LT(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);

    PlayAudioSource( Source() );
    Tick();

    EXPECT_EQ(SDL_GetAudioStreamQueued(Source().m_Stream->Get()), 64);
}

// ─── Stopping ───────────────────────────────────────────────────────────────

TEST_F(AudioSystemTest, Tick_StoppedSourceClearsItsQueuedData)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick();
    ASSERT_TRUE(Source().m_Stream->IsDataAvailable());

    StopAudioSource( Source() );
    Tick();

    EXPECT_FALSE(Source().m_Stream->IsDataAvailable());
}

// ─── Detaching ──────────────────────────────────────────────────────────────

TEST_F(AudioSystemTest, DetachAudioSource_WithNoStreamIsANoOp)
{
    auto result = DetachAudioSource( m_Device, Source() );

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(Source().m_Stream, nullptr);
}

TEST_F(AudioSystemTest, DetachAudioSource_ReleasesTheStreamAndClearsIt)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick(); // creates the stream
    ASSERT_NE(Source().m_Stream, nullptr);
    ASSERT_EQ(m_Device.Size(), 1u);

    auto result = DetachAudioSource( m_Device, Source() );

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(Source().m_Stream, nullptr);
    EXPECT_FALSE(Source().m_Playing);
    EXPECT_EQ(m_Device.Size(), 0u); // slot given back to the pool
}

TEST_F(AudioSystemTest, DetachAudioSource_ThenReplayCreatesAFreshStream)
{
    Source().m_Clip = MakeClipAsset(64);
    PlayAudioSource( Source() );
    Tick();
    auto originalStream = Source().m_Stream;
    ASSERT_TRUE(DetachAudioSource( m_Device, Source() ).IsOk());

    PlayAudioSource( Source() );
    Tick();

    ASSERT_NE(Source().m_Stream, nullptr);
    EXPECT_TRUE(Source().m_Stream->IsValid());
    EXPECT_NE(Source().m_Stream, originalStream); // a new stream, not the detached one
    EXPECT_EQ(m_Device.Size(), 1u);
}

}
