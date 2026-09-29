#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

// Real SDL-backed coverage proving SDLRenderer::SetCamera/SetViewport
// actually transform where pixels land, not just that the getters echo back
// what was set -- see SDLHeadlessFixture.hpp for why this is safe in CI.
namespace
{

using asge::graphics::RGBA_Color;
using asge::video::Camera;
using asge::video::SDLRenderer;
using asge::video::Viewport;

using SDLRendererCameraTest = asge::test::SDLHeadlessTest;

constexpr RGBA_Color kBlack{0, 0, 0, 255};
constexpr RGBA_Color kRed{255, 0, 0, 255};

bool IsRedAt(SDL_Window* inWindow, int inX, int inY)
{
    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(inWindow), nullptr);
    if (!surface) return false;

    Uint8 r, g, b, a;
    bool const read = SDL_ReadSurfacePixel(surface, inX, inY, &r, &g, &b, &a);
    SDL_DestroySurface(surface);

    return read && r > 200 && g < 50 && b < 50;
}

// ─── GetCamera / GetViewport defaults ───────────────────────────────────────

TEST_F(SDLRendererCameraTest, DefaultCamera_IsIdentity)
{
    SDLRenderer renderer(m_Window);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 1.0f);
}

TEST_F(SDLRendererCameraTest, DefaultViewport_MatchesTheWindowSize)
{
    // A zero-size default would cull every sprite outright under
    // RenderSystem's visible-rect culling (see VisibleWorldRect) unless a
    // caller remembered to call SetViewport itself -- defaulting to the
    // window's own size (64x64, per SDLHeadlessTest) means "do nothing"
    // still renders everything on screen, same as before culling existed.
    SDLRenderer renderer(m_Window);

    EXPECT_FLOAT_EQ(renderer.GetViewport().m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Y, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Width, 64.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Height, 64.0f);
}

// ─── SetCamera / SetViewport round-trip ─────────────────────────────────────

TEST_F(SDLRendererCameraTest, SetCamera_IsReturnedByGetCamera)
{
    SDLRenderer renderer(m_Window);

    Camera const camera{ .m_X = 12.0f, .m_Y = -8.0f, .m_Zoom = 2.5f };
    renderer.SetCamera(camera);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 12.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, -8.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 2.5f);
}

TEST_F(SDLRendererCameraTest, SetViewport_IsReturnedByGetViewport)
{
    SDLRenderer renderer(m_Window);

    Viewport const viewport{ 4.0f, 8.0f, 32.0f, 16.0f };
    renderer.SetViewport(viewport);

    EXPECT_FLOAT_EQ(renderer.GetViewport().m_X, 4.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Y, 8.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Width, 32.0f);
    EXPECT_FLOAT_EQ(renderer.GetViewport().m_Height, 16.0f);
}

// ─── Camera actually shifts where DrawRect lands ────────────────────────────

TEST_F(SDLRendererCameraTest, IdentityCamera_DrawRectLandsAtWorldCoordinates)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    renderer.Clear(kBlack);
    renderer.DrawRect(asge::math::Rect{0.0f, 0.0f, 10.0f, 10.0f}, kRed, true);
    renderer.Present();

    EXPECT_TRUE(IsRedAt(m_Window, 5, 5));
}

TEST_F(SDLRendererCameraTest, OffsetCamera_TranslatesDrawRectByCameraPosition)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    // Camera sits at world (20,20); a rect drawn at that same world position
    // should land at screen (0,0) -- not at (20,20).
    renderer.SetCamera(Camera{ .m_X = 20.0f, .m_Y = 20.0f, .m_Zoom = 1.0f });

    renderer.Clear(kBlack);
    renderer.DrawRect(asge::math::Rect{20.0f, 20.0f, 10.0f, 10.0f}, kRed, true);
    renderer.Present();

    EXPECT_TRUE(IsRedAt(m_Window, 5, 5))   << "rect should be drawn at screen (0,0), following the camera";
    EXPECT_FALSE(IsRedAt(m_Window, 25, 25)) << "rect should not still be at its own world coordinates";
}

TEST_F(SDLRendererCameraTest, ZoomedCamera_ScalesDrawRectSize)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    renderer.SetCamera(Camera{ .m_X = 0.0f, .m_Y = 0.0f, .m_Zoom = 2.0f });

    renderer.Clear(kBlack);
    renderer.DrawRect(asge::math::Rect{0.0f, 0.0f, 10.0f, 10.0f}, kRed, true);
    renderer.Present();

    // The 10x10 world rect covers screen (0,0)-(20,20) at 2x zoom.
    EXPECT_TRUE(IsRedAt(m_Window, 15, 15));
    EXPECT_FALSE(IsRedAt(m_Window, 25, 25));
}

// ─── Viewport shifts the whole drawing surface ──────────────────────────────

TEST_F(SDLRendererCameraTest, Viewport_OffsetsWhereDrawRectLandsOnTheWindow)
{
    // SDL_RenderReadPixels(renderer, nullptr) reads back the *current
    // viewport* rather than the whole window, so the viewport is reset to
    // the full 64x64 window before reading -- this test is about where the
    // rect ended up on screen, not about SDL_RenderReadPixels' own quirks.
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    renderer.Clear(kBlack);
    renderer.SetViewport(Viewport{ 32.0f, 0.0f, 32.0f, 64.0f }); // right half of the 64x64 window
    renderer.DrawRect(asge::math::Rect{0.0f, 0.0f, 10.0f, 10.0f}, kRed, true);

    renderer.SetViewport(Viewport{ 0.0f, 0.0f, 64.0f, 64.0f });
    renderer.Present();

    EXPECT_TRUE(IsRedAt(m_Window, 35, 5))  << "viewport-local (0,0) should land at window (32,0)";
    EXPECT_FALSE(IsRedAt(m_Window, 5, 5))  << "left half (outside the viewport at draw time) should be untouched";
}

}
