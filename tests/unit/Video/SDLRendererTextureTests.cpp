#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Video/Graphics/Rendering/RenderError.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Graphics/Font.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <utility>

// Real SDL-backed coverage for IRenderer's texture pipeline (CreateTexture +
// the DrawTexture family, including DrawText), against SDL's dummy driver +
// software renderer. Pixel-readback assertions (via SDL_RenderReadPixels/
// SDL_ReadSurfacePixel) prove these calls actually render something, not
// just that they don't crash -- see SDLHeadlessFixture.hpp for why this is
// safe in CI.
namespace
{

using asge::graphics::Font;
using asge::graphics::Image;
using asge::graphics::PixelFormat;
using asge::graphics::RGBA_Color;
using asge::video::ITexture;
using asge::video::SDLRenderer;

std::filesystem::path AhemPath()
{
    // See tests/support/fonts/NOTICE.md.
    return std::filesystem::path(ASGE_TEST_FONTS_DIR) / "Ahem.ttf";
}

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
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 2.0f, 2.0f}, asge::math::Rect{0.0f, 0.0f, 32.0f, 32.0f});
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
        renderer.DrawTexture(invalid, asge::math::Rect{0.0f, 0.0f, 1.0f, 1.0f}, asge::math::Rect{0.0f, 0.0f, 4.0f, 4.0f});
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

// DrawText builds entirely on DrawTexture(src,dst) + SetColorMod, both
// already covered above against fabricated images; these tests use a real
// baked Font (Ahem.ttf, see tests/support/fonts/NOTICE.md) so the on-screen
// glyph rect comes from Font::GetGlyph itself rather than being guessed --
// robust regardless of bake size, and pixel-predictable since every Ahem
// glyph is a solid square filling its advance width.
class DrawTextTest : public asge::test::SDLHeadlessTest
{
protected:
    static constexpr int kPixelHeight = 16; // keeps glyphs well inside the 64x64 window

    [[nodiscard]] Font LoadAhem() const
    {
        auto result = Font::Load(AhemPath(), kPixelHeight);
        EXPECT_TRUE(result.IsOk());
        return std::move(result).Value();
    }
};

TEST_F(DrawTextTest, RendersGlyphPixelsTintedByRequestedColor)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    Font font = LoadAhem();
    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_TRUE(atlasTexture->IsValid());

    auto glyphResult = font.GetGlyph(U'A');
    ASSERT_TRUE(glyphResult.IsOk());
    auto const& glyph = glyphResult.Value();

    asge::math::Float2 const pen{10.0f, 40.0f}; // baseline pen position
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawText("A", font, *atlasTexture, pen, RGBA_Color{0, 200, 0, 255});
    renderer.Present();

    // Sample the center of the glyph's actual on-screen rect, derived from
    // its own baked metrics rather than assumed.
    int const sampleX = static_cast<int>(pen.x() + static_cast<float>(glyph.bearing.x())
        + static_cast<float>(glyph.size.x()) / 2.0f);
    int const sampleY = static_cast<int>(pen.y() + static_cast<float>(glyph.bearing.y())
        + static_cast<float>(glyph.size.y()) / 2.0f);

    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(m_Window), nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, sampleX, sampleY, &r, &g, &b, &a));
    EXPECT_LT(r, 50);
    EXPECT_GT(g, 150);
    EXPECT_LT(b, 50);

    SDL_DestroySurface(surface);
}

TEST_F(DrawTextTest, AppliesRequestedColorAsAtlasColorMod)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    Font font = LoadAhem();
    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_TRUE(atlasTexture->IsValid());

    renderer.DrawText("A", font, *atlasTexture, asge::math::Float2{0.0f, 20.0f}, RGBA_Color{10, 20, 30, 40});

    auto modResult = atlasTexture->GetColorMod();
    ASSERT_TRUE(modResult.IsOk());
    EXPECT_EQ(modResult.Value().r, 10);
    EXPECT_EQ(modResult.Value().g, 20);
    EXPECT_EQ(modResult.Value().b, 30);
    EXPECT_EQ(modResult.Value().a, 40);
}

TEST_F(DrawTextTest, EmptyStringDrawsNothingAndLogsNoError)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    Font font = LoadAhem();
    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_TRUE(atlasTexture->IsValid());

    asge::test::CapturedStdout capture;
    renderer.DrawText("", font, *atlasTexture, asge::math::Float2{0.0f, 20.0f}, RGBA_Color{255, 255, 255, 255});
    EXPECT_TRUE(capture.Str().empty()) << capture.Str();
}

TEST_F(DrawTextTest, CodepointsOutsideBakedRangeAreSkippedWithoutError)
{
    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    Font font = LoadAhem();
    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_TRUE(atlasTexture->IsValid());

    // '\x01' is outside the baked ASCII 32-126 range; GetGlyph fails for it
    // and DrawText is expected to skip it (see SDLRenderer::DrawText).
    asge::test::CapturedStdout capture;
    renderer.DrawText("A\x01""B", font, *atlasTexture,
        asge::math::Float2{0.0f, 20.0f}, RGBA_Color{255, 255, 255, 255});
    EXPECT_TRUE(capture.Str().empty()) << capture.Str();
}

} // namespace
