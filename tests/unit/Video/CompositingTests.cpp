#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Compositing.hpp>
#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>

#include <gtest/gtest.h>

// Real SDL-backed coverage: the helpers are checked by reading back the pixels
// they leave on the (display-less) window, see SDLHeadlessFixture.hpp.
namespace
{

using asge::graphics::RGBA_Color;
using asge::video::BlendMode;
using asge::video::SDLRenderer;

using CompositingTest = asge::test::SDLHeadlessTest;

constexpr RGBA_Color kBlack{0, 0, 0, 255};
constexpr RGBA_Color kWhite{255, 255, 255, 255};
constexpr RGBA_Color kRed{255, 0, 0, 255};
constexpr RGBA_Color kGreen{0, 255, 0, 255};
constexpr RGBA_Color kGray{50, 50, 50, 255};
constexpr asge::math::Rect kFull{0.0f, 0.0f, 64.0f, 64.0f};

RGBA_Color PixelAt(SDL_Window* inWindow, int inX, int inY)
{
    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(inWindow), nullptr);
    RGBA_Color color{};
    if (surface)
    {
        SDL_ReadSurfacePixel(surface, inX, inY, &color.r, &color.g, &color.b, &color.a);
        SDL_DestroySurface(surface);
    }
    return color;
}

// A 64x64 render target filled with inColor.
std::unique_ptr<asge::video::ITexture> MakeTarget(SDLRenderer const& inRenderer, RGBA_Color inColor)
{
    auto target = inRenderer.CreateRenderTarget({64, 64});
    {
        asge::video::RenderTargetScope const scope(inRenderer, *target);
        inRenderer.Clear(inColor);
    }
    return target;
}

TEST_F(CompositingTest, CreateRenderTarget_HasTheRequestedSize)
{
    SDLRenderer renderer(m_Window);

    auto target = renderer.CreateRenderTarget({32, 16});

    ASSERT_TRUE(target->IsValid());
    EXPECT_EQ(target->Size().x(), 32);
    EXPECT_EQ(target->Size().y(), 16);
}

TEST_F(CompositingTest, BlendMode_RoundTripsThroughTheTexture)
{
    SDLRenderer renderer(m_Window);
    auto target = renderer.CreateRenderTarget({8, 8});

    for (BlendMode const mode : {BlendMode::None, BlendMode::Alpha, BlendMode::Additive, BlendMode::Modulate})
    {
        target->SetBlendMode(mode);
        auto const result = target->GetBlendMode();
        ASSERT_TRUE(result);
        EXPECT_EQ(result.Value(), mode);
    }
}

TEST_F(CompositingTest, RenderTarget_DrawsIntoTheTextureNotTheWindow)
{
    SDLRenderer renderer(m_Window);
    renderer.Clear(kBlack);

    auto target = MakeTarget(renderer, kRed);

    EXPECT_EQ(PixelAt(m_Window, 10, 10).r, 0); // window untouched until the target is drawn
    renderer.DrawTexture(*target, kFull);
    EXPECT_EQ(PixelAt(m_Window, 10, 10).r, 255);
}

TEST_F(CompositingTest, RenderTargetScope_RestoresTheWindowOnExit)
{
    SDLRenderer renderer(m_Window);
    auto target = renderer.CreateRenderTarget({64, 64});

    {
        asge::video::RenderTargetScope const scope(renderer, *target);
    }
    renderer.Clear(kGreen);

    EXPECT_EQ(PixelAt(m_Window, 5, 5).g, 255);
}

TEST_F(CompositingTest, DrawTextureTinted_MultipliesThePixels)
{
    SDLRenderer renderer(m_Window);
    auto target = MakeTarget(renderer, kWhite);
    renderer.Clear(kBlack);

    asge::video::DrawTextureTinted(renderer, *target, kFull, kGreen);

    auto const pixel = PixelAt(m_Window, 10, 10);
    EXPECT_EQ(pixel.r, 0);
    EXPECT_EQ(pixel.g, 255);
    EXPECT_EQ(pixel.b, 0);
}

TEST_F(CompositingTest, DrawTextureTinted_RestoresTheTextureColorMod)
{
    SDLRenderer renderer(m_Window);
    auto target = MakeTarget(renderer, kWhite);

    asge::video::DrawTextureTinted(renderer, *target, kFull, kGreen);

    auto const color = target->GetColorMod();
    ASSERT_TRUE(color);
    EXPECT_EQ(color.Value().r, 255);
    EXPECT_EQ(color.Value().g, 255);
    EXPECT_EQ(color.Value().b, 255);
}

TEST_F(CompositingTest, DrawTextureGlow_BrightensWhatIsUnderneath)
{
    SDLRenderer renderer(m_Window);
    auto target = MakeTarget(renderer, kRed);
    renderer.Clear(kGray);

    asge::video::DrawTextureGlow(renderer, *target, kFull, 1.0f);

    auto const pixel = PixelAt(m_Window, 10, 10);
    EXPECT_EQ(pixel.r, 255);  // 50 + 255, saturated
    EXPECT_EQ(pixel.g, 50);   // red target adds nothing here
}

TEST_F(CompositingTest, DrawTextureGlow_ZeroIntensityAddsNothing)
{
    SDLRenderer renderer(m_Window);
    auto target = MakeTarget(renderer, kRed);
    renderer.Clear(kGray);

    asge::video::DrawTextureGlow(renderer, *target, kFull, 0.0f);

    EXPECT_EQ(PixelAt(m_Window, 10, 10).r, 50);
}

TEST_F(CompositingTest, DrawTextureGlow_RestoresTheBlendMode)
{
    SDLRenderer renderer(m_Window);
    auto target = MakeTarget(renderer, kRed);
    target->SetBlendMode(BlendMode::Alpha);

    asge::video::DrawTextureGlow(renderer, *target, kFull, 1.0f);

    auto const mode = target->GetBlendMode();
    ASSERT_TRUE(mode);
    EXPECT_EQ(mode.Value(), BlendMode::Alpha);
}

TEST_F(CompositingTest, DrawFade_BlendsTheOverlayByTheAmount)
{
    SDLRenderer renderer(m_Window);
    renderer.Clear(kWhite);

    asge::video::DrawFade(renderer, kBlack, 0.5f);

    auto const pixel = PixelAt(m_Window, 10, 10);
    EXPECT_NEAR(pixel.r, 128, 2);
    EXPECT_NEAR(pixel.g, 128, 2);
}

TEST_F(CompositingTest, DrawFade_ZeroAmountLeavesTheFrameAlone)
{
    SDLRenderer renderer(m_Window);
    renderer.Clear(kWhite);

    asge::video::DrawFade(renderer, kBlack, 0.0f);

    EXPECT_EQ(PixelAt(m_Window, 10, 10).r, 255);
}

TEST_F(CompositingTest, DrawFade_FullAmountCoversTheFrameDespiteTheCamera)
{
    SDLRenderer renderer(m_Window);
    renderer.SetCamera({.m_X = 500.0f, .m_Y = 500.0f, .m_Zoom = 3.0f});
    renderer.Clear(kWhite);

    asge::video::DrawFade(renderer, kBlack, 1.0f);

    EXPECT_EQ(PixelAt(m_Window, 60, 60).r, 0);
}

TEST_F(CompositingTest, DrawFade_RestoresTheCamera)
{
    SDLRenderer renderer(m_Window);
    renderer.SetCamera({.m_X = 12.0f, .m_Y = 34.0f, .m_Zoom = 2.0f});

    asge::video::DrawFade(renderer, kBlack, 0.5f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 12.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 34.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 2.0f);
}

} // namespace
