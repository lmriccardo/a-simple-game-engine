#include <ASGE/Game/Components/PathFollow.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::game::components::PathFollow;
using asge::game::components::RebuildPath;
using asge::math::Float2;

TEST(RebuildPathTest, BuildsSegmentsFromTheCurrentWaypoints)
{
    PathFollow pathFollow;
    pathFollow.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{10.0f, 10.0f} };

    RebuildPath(pathFollow);

    EXPECT_TRUE(pathFollow.m_Path.HasSegments());
}

TEST(RebuildPathTest, ReflectsWaypointsChangedAfterAnEarlierBuild)
{
    PathFollow pathFollow;
    pathFollow.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f} };
    RebuildPath(pathFollow);
    float const originalLength = pathFollow.m_Path.Length();

    // Simulates the level editor dragging/adding a waypoint after the
    // initial build -- asset::Resolver<PathFollow> would never notice this
    // (it only ever builds m_Path once), so RebuildPath must be called
    // again explicitly for m_Path to stay in sync.
    pathFollow.m_Waypoints.push_back(Float2{10.0f, 100.0f});
    RebuildPath(pathFollow);

    EXPECT_GT(pathFollow.m_Path.Length(), originalLength);
}

TEST(RebuildPathTest, UsesTheCurrentResolution)
{
    PathFollow pathFollow;
    pathFollow.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f}, Float2{10.0f, 10.0f}, Float2{0.0f, 10.0f} };
    pathFollow.m_Resolution = 4;

    RebuildPath(pathFollow);

    ASSERT_TRUE(pathFollow.m_Path.HasSegments());
    EXPECT_EQ(pathFollow.m_Path.Segments().front().m_Table.size(), pathFollow.m_Resolution + 1);
}

TEST(RebuildPathTest, ResetsTravelledProgressAndFinishedFlag)
{
    PathFollow pathFollow;
    pathFollow.m_Waypoints = { Float2{0.0f, 0.0f}, Float2{10.0f, 0.0f} };
    RebuildPath(pathFollow);
    pathFollow.m_Traveled = 5.0f;
    pathFollow.m_Finished = true;

    RebuildPath(pathFollow);

    EXPECT_FLOAT_EQ(pathFollow.m_Traveled, 0.0f);
    EXPECT_FALSE(pathFollow.m_Finished);
}

TEST(RebuildPathTest, FewerThanTwoWaypoints_LeavesPathWithNoSegments)
{
    PathFollow pathFollow;
    pathFollow.m_Waypoints = { Float2{0.0f, 0.0f} };

    RebuildPath(pathFollow);

    EXPECT_FALSE(pathFollow.m_Path.HasSegments());
}

}
