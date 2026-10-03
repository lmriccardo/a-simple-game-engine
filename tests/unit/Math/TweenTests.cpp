#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::math::Ease;
using asge::math::Easing;
using asge::math::Float2;
using asge::math::Tween;

constexpr Easing kAllEasings[] = {
    Easing::Linear, Easing::InQuad, Easing::OutQuad, Easing::InOutQuad, Easing::OutCubic,
};

// ─── Ease ───────────────────────────────────────────────────────────────────

TEST(EaseTest, EveryCurve_StartsAtZeroAndEndsAtOne)
{
    for (auto const easing : kAllEasings)
    {
        EXPECT_FLOAT_EQ(Ease(easing, 0.0f), 0.0f);
        EXPECT_FLOAT_EQ(Ease(easing, 1.0f), 1.0f);
    }
}

TEST(EaseTest, EveryCurve_NeverDecreases)
{
    for (auto const easing : kAllEasings)
    {
        float previous = Ease(easing, 0.0f);
        for (int i = 1; i <= 100; ++i)
        {
            float const current = Ease(easing, static_cast<float>(i) / 100.0f);
            EXPECT_GE(current, previous);
            previous = current;
        }
    }
}

TEST(EaseTest, InputOutsideZeroToOne_IsClamped)
{
    EXPECT_FLOAT_EQ(Ease(Easing::InQuad, -3.0f), 0.0f);
    EXPECT_FLOAT_EQ(Ease(Easing::InQuad, 5.0f), 1.0f);
}

TEST(EaseTest, Midpoint_MatchesEachCurvesShape)
{
    EXPECT_FLOAT_EQ(Ease(Easing::Linear, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Ease(Easing::InQuad, 0.5f), 0.25f);
    EXPECT_FLOAT_EQ(Ease(Easing::OutQuad, 0.5f), 0.75f);
    EXPECT_FLOAT_EQ(Ease(Easing::InOutQuad, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Ease(Easing::OutCubic, 0.5f), 0.875f);
}

// ─── Tween ──────────────────────────────────────────────────────────────────

TEST(TweenTest, BeforeAnyUpdate_ValueIsTheStartAndItIsNotFinished)
{
    Tween<float> tween(10.0f, 20.0f, 2.0f);

    EXPECT_FLOAT_EQ(tween.Value(), 10.0f);
    EXPECT_FALSE(tween.IsFinished());
}

TEST(TweenTest, Update_AdvancesLinearlyAndReportsFinishOnlyOnTheFinishingCall)
{
    Tween<float> tween(0.0f, 10.0f, 2.0f);

    EXPECT_FALSE(tween.Update(1.0f));
    EXPECT_FLOAT_EQ(tween.Value(), 5.0f);
    EXPECT_TRUE(tween.Update(1.0f));
    EXPECT_FLOAT_EQ(tween.Value(), 10.0f);
    EXPECT_FALSE(tween.Update(1.0f)); // already finished -- not reported again
}

TEST(TweenTest, LargeDeltaTime_StopsAtTheEndValue)
{
    Tween<float> tween(0.0f, 10.0f, 1.0f, Easing::InQuad);

    EXPECT_TRUE(tween.Update(100.0f));
    EXPECT_FLOAT_EQ(tween.Value(), 10.0f);
}

TEST(TweenTest, ZeroDuration_IsFinishedAtOnceAtTheEndValue)
{
    Tween<float> tween(1.0f, 9.0f, 0.0f);

    EXPECT_TRUE(tween.IsFinished());
    EXPECT_FLOAT_EQ(tween.Value(), 9.0f);
}

TEST(TweenTest, EasingShapesTheValueOverTime)
{
    Tween<float> tween(0.0f, 100.0f, 2.0f, Easing::InQuad);

    tween.Update(1.0f); // halfway through time, a quarter of the way in value
    EXPECT_FLOAT_EQ(tween.Value(), 25.0f);
}

TEST(TweenTest, Reset_RewindsToTheStart)
{
    Tween<float> tween(0.0f, 10.0f, 1.0f);
    tween.Update(1.0f);

    tween.Reset();

    EXPECT_FALSE(tween.IsFinished());
    EXPECT_FLOAT_EQ(tween.Value(), 0.0f);
}

TEST(TweenTest, Float2_InterpolatesEachComponent)
{
    Tween<Float2> tween(Float2{ 0.0f, 10.0f }, Float2{ 10.0f, 30.0f }, 1.0f);

    tween.Update(0.5f);

    EXPECT_FLOAT_EQ(tween.Value().x(), 5.0f);
    EXPECT_FLOAT_EQ(tween.Value().y(), 20.0f);
}

}
