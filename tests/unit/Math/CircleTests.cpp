#include <ASGE/Core/Math/Geometry/Circle.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace
{

using asge::math::Int2;
using asge::math::MidpointCirclePoints;

bool Contains(std::vector<Int2> const& inPoints, Int2 inPoint)
{
    return std::any_of(inPoints.begin(), inPoints.end(), [&](Int2 const& inCandidate) {
        return inCandidate.x() == inPoint.x() && inCandidate.y() == inPoint.y();
    });
}

TEST(MidpointCirclePoints, ZeroRadiusReturnsOnlyTheCenterPoint)
{
    auto points = MidpointCirclePoints(Int2{5, 5}, 0);

    ASSERT_FALSE(points.empty());
    for (auto const& point : points)
    {
        EXPECT_EQ(point.x(), 5);
        EXPECT_EQ(point.y(), 5);
    }
}

TEST(MidpointCirclePoints, NegativeRadiusReturnsNoPoints)
{
    auto points = MidpointCirclePoints(Int2{0, 0}, -3);
    EXPECT_TRUE(points.empty());
}

TEST(MidpointCirclePoints, IncludesTheFourCardinalPoints)
{
    Int2 center{10, 10};
    int radius = 8;
    auto points = MidpointCirclePoints(center, radius);

    EXPECT_TRUE(Contains(points, Int2{center.x() + radius, center.y()}));
    EXPECT_TRUE(Contains(points, Int2{center.x() - radius, center.y()}));
    EXPECT_TRUE(Contains(points, Int2{center.x(), center.y() + radius}));
    EXPECT_TRUE(Contains(points, Int2{center.x(), center.y() - radius}));
}

TEST(MidpointCirclePoints, AllPointsStayWithinOnePixelOfTheTrueRadius)
{
    Int2 center{0, 0};
    int radius = 20;
    auto points = MidpointCirclePoints(center, radius);

    ASSERT_FALSE(points.empty());
    for (auto const& point : points)
    {
        double distance = std::sqrt(
            static_cast<double>(point.x() * point.x() + point.y() * point.y()));
        EXPECT_NEAR(distance, static_cast<double>(radius), 1.0);
    }
}

TEST(MidpointCirclePoints, IsSymmetricAcrossBothAxesAndTheDiagonal)
{
    Int2 center{0, 0};
    int radius = 12;
    auto points = MidpointCirclePoints(center, radius);

    for (auto const& point : points)
    {
        int x = point.x();
        int y = point.y();

        EXPECT_TRUE(Contains(points, Int2{-x,  y}));
        EXPECT_TRUE(Contains(points, Int2{ x, -y}));
        EXPECT_TRUE(Contains(points, Int2{-x, -y}));
        EXPECT_TRUE(Contains(points, Int2{ y,  x}));
    }
}

TEST(MidpointCirclePoints, TranslatingTheCenterTranslatesEveryPoint)
{
    int radius = 9;
    Int2 offset{100, -50};

    auto originPoints = MidpointCirclePoints(Int2{0, 0}, radius);
    auto shiftedPoints = MidpointCirclePoints(offset, radius);

    ASSERT_EQ(originPoints.size(), shiftedPoints.size());
    for (auto const& point : originPoints)
    {
        EXPECT_TRUE(Contains(shiftedPoints, Int2{point.x() + offset.x(), point.y() + offset.y()}));
    }
}

}
