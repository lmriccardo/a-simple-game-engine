#include <ASGE/Core/Math/Geometry/Rect.hpp>

#include <gtest/gtest.h>

namespace
{

using namespace asge::math;

// ─── AabbOverlap ─────────────────────────────────────────────────────────────

TEST(AabbOverlapTest, OverlappingBoxes_ReturnsTrue)
{
    Rect const a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect const b{5.0f, 5.0f, 10.0f, 10.0f};

    EXPECT_TRUE(AabbOverlap(a, b));
}

TEST(AabbOverlapTest, FullyContainedBox_ReturnsTrue)
{
    Rect const a{0.0f, 0.0f, 20.0f, 20.0f};
    Rect const b{5.0f, 5.0f, 5.0f, 5.0f};

    EXPECT_TRUE(AabbOverlap(a, b));
}

TEST(AabbOverlapTest, TouchingEdgeOnly_ReturnsFalse)
{
    // Zero-area overlap at a shared edge doesn't count -- AabbOverlap uses
    // strict inequalities, so exactly-adjacent boxes are "not overlapping".
    Rect const a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect const touchingOnX{10.0f, 0.0f, 10.0f, 10.0f};
    Rect const touchingOnY{0.0f, 10.0f, 10.0f, 10.0f};

    EXPECT_FALSE(AabbOverlap(a, touchingOnX));
    EXPECT_FALSE(AabbOverlap(a, touchingOnY));
}

TEST(AabbOverlapTest, SeparatedOnXAxis_ReturnsFalse)
{
    Rect const a{0.0f, 0.0f, 5.0f, 5.0f};
    Rect const b{10.0f, 0.0f, 5.0f, 5.0f};

    EXPECT_FALSE(AabbOverlap(a, b));
}

TEST(AabbOverlapTest, SeparatedOnYAxis_ReturnsFalse)
{
    Rect const a{0.0f, 0.0f, 5.0f, 5.0f};
    Rect const b{0.0f, 10.0f, 5.0f, 5.0f};

    EXPECT_FALSE(AabbOverlap(a, b));
}

// ─── PenetrationVector ───────────────────────────────────────────────────────

TEST(PenetrationVectorTest, NonOverlappingBoxes_ReturnsNullopt)
{
    Rect const a{0.0f, 0.0f, 5.0f, 5.0f};
    Rect const b{10.0f, 0.0f, 5.0f, 5.0f};

    EXPECT_FALSE(PenetrationVector(a, b).has_value());
}

TEST(PenetrationVectorTest, LesserOverlapOnX_ResolvesAlongXAxis)
{
    // a: x[0,10] y[0,10]; b: x[8,18] y[0,4] -- overlapX=2, overlapY=4, so
    // the smaller push (X) is the one returned.
    Rect const a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect const b{8.0f, 0.0f, 10.0f, 4.0f};

    auto const mtv = PenetrationVector(a, b);
    ASSERT_TRUE(mtv.has_value());
    EXPECT_FLOAT_EQ(mtv->x(), -2.0f); // a's center is left of b's -> pushed further left
    EXPECT_FLOAT_EQ(mtv->y(), 0.0f);
}

TEST(PenetrationVectorTest, LesserOverlapOnY_ResolvesAlongYAxis)
{
    // a: x[0,10] y[0,10]; b: x[0,4] y[8,18] -- overlapX=4, overlapY=2, so
    // the smaller push (Y) is the one returned.
    Rect const a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect const b{0.0f, 8.0f, 4.0f, 10.0f};

    auto const mtv = PenetrationVector(a, b);
    ASSERT_TRUE(mtv.has_value());
    EXPECT_FLOAT_EQ(mtv->x(), 0.0f);
    EXPECT_FLOAT_EQ(mtv->y(), -2.0f); // a's center is above b's -> pushed further up
}

TEST(PenetrationVectorTest, SignPointsAAwayFromB_WhenAIsOnTheOppositeSide)
{
    // Mirror of LesserOverlapOnX_ResolvesAlongXAxis with a and b swapped
    // left-to-right, to check the sign flips rather than being hardcoded.
    Rect const a{8.0f, 0.0f, 10.0f, 10.0f};
    Rect const b{0.0f, 0.0f, 10.0f, 4.0f};

    auto const mtv = PenetrationVector(a, b);
    ASSERT_TRUE(mtv.has_value());
    EXPECT_FLOAT_EQ(mtv->x(), 2.0f); // a's center is right of b's -> pushed further right
    EXPECT_FLOAT_EQ(mtv->y(), 0.0f);
}

}
