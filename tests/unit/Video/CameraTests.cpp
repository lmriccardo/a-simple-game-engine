#include <ASGE/Video/Graphics/Camera.hpp>

#include <gtest/gtest.h>

// Pure coordinate-math coverage for Camera.hpp's free functions -- no SDL
// involved. SDLRendererCameraTests.cpp covers the same math wired through a
// real IRenderer (SetCamera/SetViewport + actual pixel readback).
namespace
{

using asge::video::Camera;
using asge::video::Viewport;
using asge::video::WorldToScreen;
using asge::video::ScreenToWorld;
using asge::video::TransformRect;
using asge::video::VisibleWorldRect;

// ─── WorldToScreen (camera only) ────────────────────────────────────────────

TEST(WorldToScreenTest, IdentityCamera_ReturnsPointUnchanged)
{
    Camera const camera{};
    auto const screen = WorldToScreen(camera, asge::math::Float2{12.0f, 34.0f});

    EXPECT_FLOAT_EQ(screen.x(), 12.0f);
    EXPECT_FLOAT_EQ(screen.y(), 34.0f);
}

TEST(WorldToScreenTest, OffsetCamera_TranslatesByCameraPosition)
{
    Camera const camera{ .m_X = 10.0f, .m_Y = 20.0f, .m_Zoom = 1.0f };
    auto const screen = WorldToScreen(camera, asge::math::Float2{15.0f, 25.0f});

    EXPECT_FLOAT_EQ(screen.x(), 5.0f); // 15 - 10
    EXPECT_FLOAT_EQ(screen.y(), 5.0f); // 25 - 20
}

TEST(WorldToScreenTest, ZoomedCamera_ScalesAfterTranslating)
{
    Camera const camera{ .m_X = 5.0f, .m_Y = 5.0f, .m_Zoom = 2.0f };
    auto const screen = WorldToScreen(camera, asge::math::Float2{10.0f, 5.0f});

    EXPECT_FLOAT_EQ(screen.x(), 10.0f); // (10 - 5) * 2
    EXPECT_FLOAT_EQ(screen.y(), 0.0f);  // (5 - 5) * 2
}

// ─── WorldToScreen (camera + viewport) ──────────────────────────────────────

TEST(WorldToScreenViewportTest, OffsetsByViewportOriginOnTopOfTheCamera)
{
    Camera const camera{ .m_X = 10.0f, .m_Y = 20.0f, .m_Zoom = 1.0f };
    Viewport const viewport{ 100.0f, 50.0f, 800.0f, 600.0f };
    auto const screen = WorldToScreen(camera, viewport, asge::math::Float2{15.0f, 25.0f});

    EXPECT_FLOAT_EQ(screen.x(), 105.0f); // (15 - 10) + 100
    EXPECT_FLOAT_EQ(screen.y(), 55.0f);  // (25 - 20) + 50
}

TEST(WorldToScreenViewportTest, ZeroOriginViewport_MatchesTheCameraOnlyOverload)
{
    Camera const camera{ .m_X = 3.0f, .m_Y = 7.0f, .m_Zoom = 2.0f };
    Viewport const viewport{ 0.0f, 0.0f, 640.0f, 480.0f };

    auto const withViewport = WorldToScreen(camera, viewport, asge::math::Float2{20.0f, 20.0f});
    auto const withoutViewport = WorldToScreen(camera, asge::math::Float2{20.0f, 20.0f});

    EXPECT_FLOAT_EQ(withViewport.x(), withoutViewport.x());
    EXPECT_FLOAT_EQ(withViewport.y(), withoutViewport.y());
}

// ─── ScreenToWorld ───────────────────────────────────────────────────────────

TEST(ScreenToWorldTest, IsTheInverseOfWorldToScreen)
{
    Camera const camera{ .m_X = 40.0f, .m_Y = -15.0f, .m_Zoom = 2.5f };
    Viewport const viewport{ 8.0f, 16.0f, 320.0f, 240.0f };
    asge::math::Float2 const original{ 123.0f, -45.0f };

    auto const screen = WorldToScreen(camera, viewport, original);
    auto const roundTripped = ScreenToWorld(camera, viewport, screen);

    EXPECT_FLOAT_EQ(roundTripped.x(), original.x());
    EXPECT_FLOAT_EQ(roundTripped.y(), original.y());
}

TEST(ScreenToWorldTest, IdentityCameraAndZeroViewport_ReturnsPointUnchanged)
{
    Camera const camera{};
    Viewport const viewport{ 0.0f, 0.0f, 100.0f, 100.0f };

    auto const world = ScreenToWorld(camera, viewport, asge::math::Float2{9.0f, 4.0f});

    EXPECT_FLOAT_EQ(world.x(), 9.0f);
    EXPECT_FLOAT_EQ(world.y(), 4.0f);
}

// ─── TransformRect ───────────────────────────────────────────────────────────

TEST(TransformRectTest, IdentityCamera_ReturnsRectUnchanged)
{
    Camera const camera{};
    asge::math::Rect const world{ 1.0f, 2.0f, 3.0f, 4.0f };

    auto const screen = TransformRect(camera, world);

    EXPECT_FLOAT_EQ(screen.m_X, 1.0f);
    EXPECT_FLOAT_EQ(screen.m_Y, 2.0f);
    EXPECT_FLOAT_EQ(screen.m_Width, 3.0f);
    EXPECT_FLOAT_EQ(screen.m_Height, 4.0f);
}

TEST(TransformRectTest, TranslatesPositionAndScalesSizeByZoom)
{
    Camera const camera{ .m_X = 5.0f, .m_Y = 5.0f, .m_Zoom = 2.0f };
    asge::math::Rect const world{ 10.0f, 10.0f, 20.0f, 30.0f };

    auto const screen = TransformRect(camera, world);

    EXPECT_FLOAT_EQ(screen.m_X, 10.0f);      // (10 - 5) * 2
    EXPECT_FLOAT_EQ(screen.m_Y, 10.0f);      // (10 - 5) * 2
    EXPECT_FLOAT_EQ(screen.m_Width, 40.0f);  // 20 * 2
    EXPECT_FLOAT_EQ(screen.m_Height, 60.0f); // 30 * 2
}

TEST(TransformRectTest, ZeroZoom_CollapsesRectToAPointAtTheCameraOrigin)
{
    Camera const camera{ .m_X = 1.0f, .m_Y = 1.0f, .m_Zoom = 0.0f };
    asge::math::Rect const world{ 10.0f, 10.0f, 20.0f, 20.0f };

    auto const screen = TransformRect(camera, world);

    EXPECT_FLOAT_EQ(screen.m_Width, 0.0f);
    EXPECT_FLOAT_EQ(screen.m_Height, 0.0f);
}

// ─── VisibleWorldRect ────────────────────────────────────────────────────────

TEST(VisibleWorldRectTest, IdentityCamera_MatchesTheViewportSizeAtTheOrigin)
{
    Camera const camera{};
    Viewport const viewport{ 0.0f, 0.0f, 800.0f, 600.0f };

    auto const visible = VisibleWorldRect(camera, viewport);

    EXPECT_FLOAT_EQ(visible.m_X, 0.0f);
    EXPECT_FLOAT_EQ(visible.m_Y, 0.0f);
    EXPECT_FLOAT_EQ(visible.m_Width, 800.0f);
    EXPECT_FLOAT_EQ(visible.m_Height, 600.0f);
}

TEST(VisibleWorldRectTest, OffsetCamera_AnchorsTheVisibleRectAtTheCameraPosition)
{
    Camera const camera{ .m_X = 100.0f, .m_Y = 50.0f, .m_Zoom = 1.0f };
    Viewport const viewport{ 0.0f, 0.0f, 400.0f, 300.0f };

    auto const visible = VisibleWorldRect(camera, viewport);

    EXPECT_FLOAT_EQ(visible.m_X, 100.0f);
    EXPECT_FLOAT_EQ(visible.m_Y, 50.0f);
    EXPECT_FLOAT_EQ(visible.m_Width, 400.0f);
    EXPECT_FLOAT_EQ(visible.m_Height, 300.0f);
}

TEST(VisibleWorldRectTest, ZoomedInCamera_HalvesTheVisibleWorldArea)
{
    // Zooming in shows less world per screen pixel -- the visible rect
    // shrinks as zoom grows, inverse of TransformRect's size math.
    Camera const camera{ .m_Zoom = 2.0f };
    Viewport const viewport{ 0.0f, 0.0f, 800.0f, 600.0f };

    auto const visible = VisibleWorldRect(camera, viewport);

    EXPECT_FLOAT_EQ(visible.m_Width, 400.0f);  // 800 / 2
    EXPECT_FLOAT_EQ(visible.m_Height, 300.0f); // 600 / 2
}

TEST(VisibleWorldRectTest, ZoomedOutCamera_GrowsTheVisibleWorldArea)
{
    Camera const camera{ .m_Zoom = 0.5f };
    Viewport const viewport{ 0.0f, 0.0f, 800.0f, 600.0f };

    auto const visible = VisibleWorldRect(camera, viewport);

    EXPECT_FLOAT_EQ(visible.m_Width, 1600.0f);  // 800 / 0.5
    EXPECT_FLOAT_EQ(visible.m_Height, 1200.0f); // 600 / 0.5
}

}
