#include <ASGE/Game/Components/Animation.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::game::components::Animation;
using asge::game::components::EffectiveFrameDuration;
using asge::game::components::PlayClip;
using asge::game::components::StopAnimation;

Animation MakeAnimation()
{
    Animation anim{ .m_ClipPath = "clips/idle.toml" };
    anim.m_Clips = { { "idle", "clips/idle.toml" }, { "run", "clips/run.toml" } };
    return anim;
}

// ─── EffectiveFrameDuration ──────────────────────────────────────────────────

TEST(EffectiveFrameDurationTest, NoOverride_UsesComponentFrameDuration)
{
    auto anim = MakeAnimation();
    anim.m_FrameDuration = 0.25f;
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), 0.25f);
}

TEST(EffectiveFrameDurationTest, OverrideOfCurrentClip_WinsOverComponentValue)
{
    auto anim = MakeAnimation();
    anim.m_ClipFrameDurations["idle"] = 0.5f;
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), 0.5f);
}

TEST(EffectiveFrameDurationTest, OverrideOfOtherClip_IsIgnored)
{
    auto anim = MakeAnimation();
    anim.m_ClipFrameDurations["run"] = 0.05f;
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), anim.m_FrameDuration);
}

TEST(EffectiveFrameDurationTest, SwitchingClipSwitchesDuration)
{
    auto anim = MakeAnimation();
    anim.m_ClipFrameDurations = { { "idle", 0.5f }, { "run", 0.05f } };
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), 0.5f);
    ASSERT_TRUE(PlayClip(anim, "run"));
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), 0.05f);
}

TEST(EffectiveFrameDurationTest, NonPositiveOverride_FallsBackToComponentValue)
{
    auto anim = MakeAnimation();
    anim.m_ClipFrameDurations["idle"] = 0.0f;
    EXPECT_FLOAT_EQ(EffectiveFrameDuration(anim), anim.m_FrameDuration);
}

// ─── PlayClip ────────────────────────────────────────────────────────────────

TEST(PlayClipTest, KnownName_SwitchesClipPathAndRestartsPlayback)
{
    Animation anim = MakeAnimation();
    anim.m_CurrentFrame = 6;
    anim.m_ElapsedTime = 0.05f;
    anim.m_Loop = false;

    EXPECT_TRUE(PlayClip(anim, "run"));
    EXPECT_EQ(anim.m_ClipPath, "clips/run.toml");
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
    EXPECT_FLOAT_EQ(anim.m_ElapsedTime, 0.0f);
    EXPECT_TRUE(anim.m_Loop);
    EXPECT_TRUE(anim.m_Playing);
}

TEST(PlayClipTest, InLoopFalse_PlaysOnce)
{
    Animation anim = MakeAnimation();
    EXPECT_TRUE(PlayClip(anim, "run", false));
    EXPECT_FALSE(anim.m_Loop);
}

TEST(PlayClipTest, UnknownName_ReturnsFalseAndChangesNothing)
{
    Animation anim = MakeAnimation();
    anim.m_CurrentFrame = 3;

    EXPECT_FALSE(PlayClip(anim, "attack"));
    EXPECT_EQ(anim.m_ClipPath, "clips/idle.toml");
    EXPECT_EQ(anim.m_CurrentFrame, 3u);
}

TEST(PlayClipTest, AlreadyPlayingThatClip_DoesNotRestart)
{
    // Gameplay code calls PlayClip every frame while a key is held.
    Animation anim = MakeAnimation();
    anim.m_CurrentFrame = 3;
    anim.m_ElapsedTime = 0.04f;

    EXPECT_TRUE(PlayClip(anim, "idle"));
    EXPECT_EQ(anim.m_CurrentFrame, 3u);
    EXPECT_FLOAT_EQ(anim.m_ElapsedTime, 0.04f);
}

TEST(PlayClipTest, SameClipButStopped_RestartsFromFrameZero)
{
    Animation anim = MakeAnimation();
    anim.m_CurrentFrame = 3;
    StopAnimation(anim);

    EXPECT_TRUE(PlayClip(anim, "idle"));
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
    EXPECT_TRUE(anim.m_Playing);
}

} // namespace
