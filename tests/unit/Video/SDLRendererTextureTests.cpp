#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Video/Graphics/Rendering/RenderError.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

// Real SDL-backed coverage for IRenderer's texture pipeline (CreateTexture +
// the DrawTexture family), against SDL's dummy driver + software renderer.
// Pixel-readback assertions (via SDL_RenderReadPixels/SDL_ReadSurfacePixel)
// prove these calls actually render something, not just that they don't
// crash -- see SDLHeadlessFixture.hpp for why this is safe in CI.
namespace
{

using asge::graphics::Image;
using asge::graphics::PixelFormat;
using asge::graphics::RGBA_Color;
using asge::video::ITexture;
using asge::video::SDLRenderer;

Image MakeSolidImage(std::size_t inW, std::size_t inH, RGBA_Color inColor)
{
    Image::data_t pixels;
    pixels.reserve(inW * inH * 4);
    for (std::size_t i = 0; i < inW * inH; ++i)
    {
        pixels.push_back(inColor.r);
        pixels.push_back(inColor.g);
        pixels.push_back(inColor.b);
        pixels.push_back(inColor.a);
    }
    return Image(inW, inH, PixelFormat::RGBA8, pixels);
}

// ITexture that never had a real SDL handle -- used to drive SDLRenderer's
// DrawTexture* failure/RenderTextureFailed logging path.
class InvalidTexture final : public ITexture
{
public:
    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return { 0, 0 }; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return false; }

    void SetColorMod(RGBA_Color) noexcept override {}
    [[nodiscard]] asge::Result<RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<RGBA_Color>::Ok(RGBA_Color{});
    }
};

using SDLRendererTextureTest = asge::test::SDLHeadlessTest;

TEST_F(SDLRendererTextureTest, CreateTextureWrapsAUsableSDLTexture)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto image = MakeSolidImage(4, 4, RGBA_Color{255, 0, 0, 255});
    auto texture = renderer.CreateTexture(image);

    ASSERT_NE(texture, nullptr);
    EXPECT_TRUE(texture->IsValid());
    EXPECT_EQ(texture->Size().x(), 4);
    EXPECT_EQ(texture->Size().y(), 4);
}

TEST_F(SDLRendererTextureTest, DrawTextureRectFillsDestinationWithTexturePixels)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto image = MakeSolidImage(2, 2, RGBA_Color{255, 0, 0, 255});
    auto texture = renderer.CreateTexture(image);
    ASSERT_TRUE(texture->IsValid());

    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 64.0f, 64.0f});
    renderer.Present();

    // SDLRenderer doesn't expose its raw handle (deliberately -- IRenderer
    // stays backend-opaque); SDL_GetRenderer(window) retrieves the same
    // renderer SDLRenderer just created, purely for readback in the test.
    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(m_Window), nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 32, 32, &r, &g, &b, &a));
    EXPECT_GT(r, 200);
    EXPECT_LT(g, 50);
    EXPECT_LT(b, 50);

    SDL_DestroySurface(surface);
}

TEST_F(SDLRendererTextureTest, DrawTexturePositionDrawsAtNativeSizeOnly)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto image = MakeSolidImage(2, 2, RGBA_Color{255, 0, 0, 255});
    auto texture = renderer.CreateTexture(image);
    ASSERT_TRUE(texture->IsValid());

    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Float2{0.0f, 0.0f});
    renderer.Present();

    // SDLRenderer doesn't expose its raw handle (deliberately -- IRenderer
    // stays backend-opaque); SDL_GetRenderer(window) retrieves the same
    // renderer SDLRenderer just created, purely for readback in the test.
    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(m_Window), nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 0, 0, &r, &g, &b, &a));
    EXPECT_GT(r, 200) << "texture should have been drawn at its native 2x2 size";

    // Untouched past the texture's native size -- proves this overload does
    // not scale to fill, unlike DrawTexture(Rect).
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 40, 40, &r, &g, &b, &a));
    EXPECT_LT(r, 50);

    SDL_DestroySurface(surface);
}

TEST_F(SDLRendererTextureTest, RemainingDrawTextureVariantsRunWithoutError)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto image = MakeSolidImage(4, 4, RGBA_Color{0, 255, 0, 255});
    auto texture = renderer.CreateTexture(image);
    ASSERT_TRUE(texture->IsValid());

    asge::test::CapturedStdout capture;
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture9Grid(
        *texture, 1.0f, 1.0f, 1.0f, 1.0f, asge::math::Rect{0.0f, 0.0f, 32.0f, 32.0f});
    renderer.DrawTextureTiled(*texture, 1.0f, asge::math::Rect{0.0f, 0.0f, 32.0f, 32.0f});
    renderer.DrawTextureAffine(*texture,
        asge::math::Float2{0.0f, 0.0f}, asge::math::Float2{16.0f, 0.0f}, asge::math::Float2{0.0f, 16.0f});
    renderer.Present();
    auto const output = capture.Str();

    EXPECT_EQ(output.find("failed to render a texture"), std::string::npos) << output;
}

TEST_F(SDLRendererTextureTest, EveryDrawTextureVariantLogsRenderTextureFailedForAnInvalidTexture)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    InvalidTexture invalid;

    {
        asge::test::CapturedStdout capture;
        renderer.DrawTexture(invalid, asge::math::Rect{0.0f, 0.0f, 4.0f, 4.0f});
        EXPECT_NE(capture.Str().find("failed to render a texture"), std::string::npos);
    }
    {
        asge::test::CapturedStdout capture;
        renderer.DrawTexture(invalid, asge::math::Float2{0.0f, 0.0f});
        EXPECT_NE(capture.Str().find("failed to render a texture"), std::string::npos);
    }
    {
        asge::test::CapturedStdout capture;
        renderer.DrawTexture9Grid(
            invalid, 1.0f, 1.0f, 1.0f, 1.0f, asge::math::Rect{0.0f, 0.0f, 4.0f, 4.0f});
        EXPECT_NE(capture.Str().find("failed to render a texture"), std::string::npos);
    }
    {
        asge::test::CapturedStdout capture;
        renderer.DrawTextureTiled(invalid, 1.0f, asge::math::Rect{0.0f, 0.0f, 4.0f, 4.0f});
        EXPECT_NE(capture.Str().find("failed to render a texture"), std::string::npos);
    }
    {
        asge::test::CapturedStdout capture;
        renderer.DrawTextureAffine(invalid,
            asge::math::Float2{0.0f, 0.0f}, asge::math::Float2{4.0f, 0.0f}, asge::math::Float2{0.0f, 4.0f});
        EXPECT_NE(capture.Str().find("failed to render a texture"), std::string::npos);
    }
}

} // namespace
