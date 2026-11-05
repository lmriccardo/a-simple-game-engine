#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::math::AngleOf;
using asge::math::AngularDistance;
using asge::math::kPi;
using asge::math::QuantizeAngle;

// ─── AngleOf ────────────────────────────────────────────────────────────────

TEST(AngleOfTest, AxesAreCounterClockwiseFromPositiveX)
{
    EXPECT_NEAR(AngleOf(1.0f, 0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(AngleOf(0.0f, 1.0f), kPi / 2.0f, 1e-6f);
    EXPECT_NEAR(AngleOf(-1.0f, 0.0f), kPi, 1e-6f);
    EXPECT_NEAR(AngleOf(0.0f, -1.0f), 3.0f * kPi / 2.0f, 1e-6f);
}

TEST(AngleOfTest, ResultIsNeverNegative)
{
    EXPECT_GE(AngleOf(1.0f, -0.001f), 0.0f);
}

TEST(AngleOfTest, ZeroVectorReturnsZero)
{
    EXPECT_FLOAT_EQ(AngleOf(0.0f, 0.0f), 0.0f);
}

// ─── AngularDistance ────────────────────────────────────────────────────────

TEST(AngularDistanceTest, WrapsAroundTheFullTurn)
{
    EXPECT_NEAR(AngularDistance(0.1f, 2.0f * kPi - 0.1f), 0.2f, 1e-5f);
}

TEST(AngularDistanceTest, OppositeAnglesAreHalfATurnApart)
{
    EXPECT_NEAR(AngularDistance(0.0f, kPi), kPi, 1e-6f);
}

TEST(AngularDistanceTest, IsSymmetricAndZeroForEqualAngles)
{
    EXPECT_FLOAT_EQ(AngularDistance(1.0f, 2.5f), AngularDistance(2.5f, 1.0f));
    EXPECT_FLOAT_EQ(AngularDistance(1.0f, 1.0f), 0.0f);
}

TEST(AngularDistanceTest, AcceptsAnglesOutsideOneTurn)
{
    EXPECT_NEAR(AngularDistance(-0.1f, 0.1f), 0.2f, 1e-5f);
}

// ─── QuantizeAngle ──────────────────────────────────────────────────────────

TEST(QuantizeAngleTest, FourSectorsSnapToTheNearestAxis)
{
    EXPECT_EQ(QuantizeAngle(0.1f, 4), 0);
    EXPECT_EQ(QuantizeAngle(kPi / 2.0f + 0.1f, 4), 1);
    EXPECT_EQ(QuantizeAngle(kPi - 0.1f, 4), 2);
    EXPECT_EQ(QuantizeAngle(3.0f * kPi / 2.0f, 4), 3);
}

TEST(QuantizeAngleTest, SectorZeroCoversBothSidesOfPositiveX)
{
    EXPECT_EQ(QuantizeAngle(2.0f * kPi - 0.1f, 4), 0);
    EXPECT_EQ(QuantizeAngle(-0.1f, 4), 0);
}

TEST(QuantizeAngleTest, EightSectorsSplitTheDiagonals)
{
    EXPECT_EQ(QuantizeAngle(kPi / 4.0f, 8), 1);
    EXPECT_EQ(QuantizeAngle(7.0f * kPi / 4.0f, 8), 7);
}

TEST(QuantizeAngleTest, HysteresisKeepsTheCurrentSectorJustPastTheBoundary)
{
    // Boundary between sector 0 and 1 (four sectors) is at 45 degrees.
    float const pastBoundary = kPi / 4.0f + 0.05f;
    EXPECT_EQ(QuantizeAngle(pastBoundary, 4), 1);
    EXPECT_EQ(QuantizeAngle(pastBoundary, 4, 0, 1.15f), 0);
}

TEST(QuantizeAngleTest, HysteresisStillSwitchesOnceClearlyPastTheBoundary)
{
    EXPECT_EQ(QuantizeAngle(kPi / 4.0f + 0.4f, 4, 0, 1.15f), 1);
}

TEST(QuantizeAngleTest, InvalidCurrentSectorIsIgnored)
{
    EXPECT_EQ(QuantizeAngle(kPi / 4.0f + 0.05f, 4, 7, 1.15f), 1);
    EXPECT_EQ(QuantizeAngle(kPi / 4.0f + 0.05f, 4, -1, 1.15f), 1);
}

TEST(QuantizeAngleTest, NonPositiveSectorCountReturnsZero)
{
    EXPECT_EQ(QuantizeAngle(1.0f, 0), 0);
}

}
