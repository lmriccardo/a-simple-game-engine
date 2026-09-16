#include <ASGE/Core/Math/Geometry/CatmullRomSpline.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace
{

using namespace asge::math;

// ─── Hermite ─────────────────────────────────────────────────────────────────

TEST(HermiteTest, AtZero_ReturnsFirstEndpoint)
{
    Float2 const point = Hermite(Float2{-1.0f, 2.0f}, Float2{0.0f, 0.0f}, Float2{5.0f, 3.0f}, Float2{6.0f, 1.0f}, 0.0f);

    EXPECT_FLOAT_EQ(point.x(), 0.0f);
    EXPECT_FLOAT_EQ(point.y(), 0.0f);
}

TEST(HermiteTest, AtOne_ReturnsSecondEndpoint)
{
    Float2 const point = Hermite(Float2{-1.0f, 2.0f}, Float2{0.0f, 0.0f}, Float2{5.0f, 3.0f}, Float2{6.0f, 1.0f}, 1.0f);

    EXPECT_FLOAT_EQ(point.x(), 5.0f);
    EXPECT_FLOAT_EQ(point.y(), 3.0f);
}

TEST(HermiteTest, CollinearEquallySpacedWaypoints_ProducesAUniformStraightLine)
{
    Float2 const p0{-10.0f, 0.0f}, p1{0.0f, 0.0f}, p2{10.0f, 0.0f}, p3{20.0f, 0.0f};

    for (float t = 0.0f; t <= 1.0f; t += 0.25f)
    {
        Float2 const point = Hermite(p0, p1, p2, p3, t);
        EXPECT_NEAR(point.x(), 10.0f * t, 1e-3f);
        EXPECT_NEAR(point.y(), 0.0f, 1e-3f);
    }
}

// ─── HermiteDerivative ───────────────────────────────────────────────────────

TEST(HermiteDerivativeTest, AtZero_ReturnsFirstEndpointTangent)
{
    Float2 const p0{-1.0f, 2.0f}, p1{0.0f, 0.0f}, p2{5.0f, 3.0f}, p3{6.0f, 1.0f};
    Float2 const expected = (p2 - p0) * 0.5f;

    Float2 const tangent = HermiteDerivative(p0, p1, p2, p3, 0.0f);

    EXPECT_FLOAT_EQ(tangent.x(), expected.x());
    EXPECT_FLOAT_EQ(tangent.y(), expected.y());
}

TEST(HermiteDerivativeTest, AtOne_ReturnsSecondEndpointTangent)
{
    Float2 const p0{-1.0f, 2.0f}, p1{0.0f, 0.0f}, p2{5.0f, 3.0f}, p3{6.0f, 1.0f};
    Float2 const expected = (p3 - p1) * 0.5f;

    Float2 const tangent = HermiteDerivative(p0, p1, p2, p3, 1.0f);

    EXPECT_FLOAT_EQ(tangent.x(), expected.x());
    EXPECT_FLOAT_EQ(tangent.y(), expected.y());
}

// ─── BuildArcLengthTable ─────────────────────────────────────────────────────

TEST(BuildArcLengthTableTest, ProducesResolutionPlusOneSamples)
{
    std::vector<ArcLengthSample> table;
    BuildArcLengthTable(Float2{-10.0f, 0.0f}, Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, 8, table);

    EXPECT_EQ(table.size(), 9u);
}

TEST(BuildArcLengthTableTest, FirstSampleIsOriginAndLastMatchesReturnedLength)
{
    std::vector<ArcLengthSample> table;
    float const length =
        BuildArcLengthTable(Float2{-10.0f, 0.0f}, Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, 8, table);

    ASSERT_FALSE(table.empty());
    EXPECT_FLOAT_EQ(table.front().m_T, 0.0f);
    EXPECT_FLOAT_EQ(table.front().m_Distance, 0.0f);
    EXPECT_FLOAT_EQ(table.back().m_T, 1.0f);
    EXPECT_FLOAT_EQ(table.back().m_Distance, length);
    EXPECT_FALSE(std::isnan(length));
}

TEST(BuildArcLengthTableTest, DistancesAreMonotonicallyNonDecreasing)
{
    std::vector<ArcLengthSample> table;
    BuildArcLengthTable(Float2{0.0f, 0.0f}, Float2{0.0f, 0.0f}, Float2{5.0f, 8.0f}, Float2{10.0f, 0.0f}, 16, table);

    for (std::size_t ii = 1; ii < table.size(); ++ii)
    {
        EXPECT_GE(table[ii].m_Distance, table[ii - 1].m_Distance);
    }
}

TEST(BuildArcLengthTableTest, StraightSegment_LengthMatchesTheDirectDistance)
{
    std::vector<ArcLengthSample> table;
    float const length =
        BuildArcLengthTable(Float2{-10.0f, 0.0f}, Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, 16, table);

    EXPECT_NEAR(length, 10.0f, 1e-2f);
}

// ─── CatmullRomSpline — construction ─────────────────────────────────────────

TEST(CatmullRomSplineTest, FewerThanTwoWaypoints_HasNoSegmentsAndZeroLength)
{
    CatmullRomSpline const single(std::vector<Float2>{Float2{1.0f, 2.0f}});

    EXPECT_TRUE(single.Segments().empty());
    EXPECT_FLOAT_EQ(single.Length(), 0.0f);

    Float2 const point = single.PointAt(0.5f);
    EXPECT_FLOAT_EQ(point.x(), 0.0f);
    EXPECT_FLOAT_EQ(point.y(), 0.0f);
}

TEST(CatmullRomSplineTest, WaypointsAccessor_ReturnsWhatWasPassedIn)
{
    std::vector<Float2> const wp{Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    ASSERT_EQ(spline.Waypoints().size(), wp.size());
    for (std::size_t ii = 0; ii < wp.size(); ++ii)
    {
        EXPECT_FLOAT_EQ(spline.Waypoints()[ii].x(), wp[ii].x());
        EXPECT_FLOAT_EQ(spline.Waypoints()[ii].y(), wp[ii].y());
    }
}

TEST(CatmullRomSplineTest, NWaypoints_ProducesNMinusOneSegments)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    EXPECT_EQ(spline.Segments().size(), wp.size() - 1);
}

TEST(CatmullRomSplineTest, StraightEquallySpacedWaypoints_TotalLengthIsTheSumOfSegmentSpacing)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    EXPECT_NEAR(spline.Length(), 30.0f, 1e-2f);
    EXPECT_FALSE(std::isnan(spline.Length()));
}

// ─── CatmullRomSpline::PointAt ───────────────────────────────────────────────

TEST(CatmullRomSplineTest, PointAt_ZeroAndOneReturnTheEndWaypoints)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const start = spline.PointAt(0.0f);
    Float2 const end = spline.PointAt(1.0f);

    EXPECT_NEAR(start.x(), 0.0f, 1e-3f);
    EXPECT_NEAR(end.x(), 30.0f, 1e-3f);
}

TEST(CatmullRomSplineTest, PointAt_MidParameter_InterpolatesAlongTheStraightLine)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const mid = spline.PointAt(0.5f);

    EXPECT_NEAR(mid.x(), 15.0f, 1e-2f);
    EXPECT_NEAR(mid.y(), 0.0f, 1e-2f);
}

TEST(CatmullRomSplineTest, PointAt_OutOfRangeParameter_ClampsToTheEndWaypoints)
{
    std::vector<Float2> const wp{Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const belowZero = spline.PointAt(-5.0f);
    Float2 const atZero = spline.PointAt(0.0f);
    Float2 const aboveOne = spline.PointAt(5.0f);
    Float2 const atOne = spline.PointAt(1.0f);

    EXPECT_FLOAT_EQ(belowZero.x(), atZero.x());
    EXPECT_FLOAT_EQ(belowZero.y(), atZero.y());
    EXPECT_FLOAT_EQ(aboveOne.x(), atOne.x());
    EXPECT_FLOAT_EQ(aboveOne.y(), atOne.y());
}

// ─── CatmullRomSpline::PointAtDistance ───────────────────────────────────────

TEST(CatmullRomSplineTest, PointAtDistance_ZeroAndLengthReturnTheEndWaypoints)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const start = spline.PointAtDistance(0.0f);
    Float2 const end = spline.PointAtDistance(spline.Length());

    EXPECT_NEAR(start.x(), 0.0f, 1e-2f);
    EXPECT_NEAR(end.x(), 30.0f, 1e-2f);
}

TEST(CatmullRomSplineTest, PointAtDistance_HalfwayAlongAStraightLine_MatchesTheMidpoint)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const mid = spline.PointAtDistance(spline.Length() * 0.5f);

    EXPECT_NEAR(mid.x(), 15.0f, 1e-1f);
    EXPECT_NEAR(mid.y(), 0.0f, 1e-2f);
}

TEST(CatmullRomSplineTest, PointAtDistance_OutOfRangeDistance_ClampsToTheEndWaypoints)
{
    std::vector<Float2> const wp{Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const belowZero = spline.PointAtDistance(-5.0f);
    Float2 const atZero = spline.PointAtDistance(0.0f);
    Float2 const beyondLength = spline.PointAtDistance(spline.Length() + 100.0f);
    Float2 const atLength = spline.PointAtDistance(spline.Length());

    EXPECT_FLOAT_EQ(belowZero.x(), atZero.x());
    EXPECT_FLOAT_EQ(belowZero.y(), atZero.y());
    EXPECT_FLOAT_EQ(beyondLength.x(), atLength.x());
    EXPECT_FLOAT_EQ(beyondLength.y(), atLength.y());
}

// ─── CatmullRomSpline::TangentAt ─────────────────────────────────────────────

TEST(CatmullRomSplineTest, TangentAt_IsNormalized)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 4.0f}, Float2{20.0f, -2.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const tangent = spline.TangentAt(0.5f);

    EXPECT_NEAR(Length(tangent), 1.0f, 1e-3f);
}

TEST(CatmullRomSplineTest, TangentAt_OnAStraightLine_PointsAlongTheLine)
{
    std::vector<Float2> const wp{
        Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{20.0f, 0.0f}, Float2{30.0f, 0.0f}};
    CatmullRomSpline const spline(wp);

    Float2 const tangent = spline.TangentAt(0.5f);

    EXPECT_NEAR(tangent.x(), 1.0f, 1e-3f);
    EXPECT_NEAR(tangent.y(), 0.0f, 1e-3f);
}

TEST(CatmullRomSplineTest, TangentAt_EmptySpline_ReturnsZeroWithoutProducingNaN)
{
    CatmullRomSpline const empty(std::vector<Float2>{});

    Float2 const tangent = empty.TangentAt(0.5f);

    EXPECT_FLOAT_EQ(tangent.x(), 0.0f);
    EXPECT_FLOAT_EQ(tangent.y(), 0.0f);
}

}
