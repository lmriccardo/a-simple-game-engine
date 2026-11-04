#include <ASGE/Game/Components/Path.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::game::components::Path;
using asge::game::components::RebuildPath;
using asge::math::Float2;

TEST(RebuildPathTest, BuildsSegmentsFromTheCurrentWaypoints)
{
    Path path;
    path.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{10.0f, 10.0f} };

    RebuildPath(path);

    EXPECT_TRUE(path.m_Path.HasSegments());
}

TEST(RebuildPathTest, ReflectsWaypointsChangedAfterAnEarlierBuild)
{
    Path path;
    path.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f} };
    RebuildPath(path);
    float const originalLength = path.m_Path.Length();

    // Simulates the level editor dragging/adding a waypoint after the
    // initial build -- asset::Resolver<Path> would never notice this
    // (it only ever builds m_Path once), so RebuildPath must be called
    // again explicitly for m_Path to stay in sync.
    path.m_Waypoints.push_back(Float2{10.0f, 100.0f});
    RebuildPath(path);

    EXPECT_GT(path.m_Path.Length(), originalLength);
}

TEST(RebuildPathTest, UsesTheCurrentResolution)
{
    Path path;
    path.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{10.0f, 10.0f}, Float2{0.0f, 10.0f} };
    path.m_Resolution = 4;

    RebuildPath(path);

    ASSERT_TRUE(path.m_Path.HasSegments());
    EXPECT_EQ(path.m_Path.Segments().front().m_Table.size(), path.m_Resolution + 1);
}

TEST(RebuildPathTest, FewerThanTwoWaypoints_LeavesPathWithNoSegments)
{
    Path path;
    path.m_Waypoints = { Float2{0.0f, 0.0f} };

    RebuildPath(path);

    EXPECT_FALSE(path.m_Path.HasSegments());
}

}
