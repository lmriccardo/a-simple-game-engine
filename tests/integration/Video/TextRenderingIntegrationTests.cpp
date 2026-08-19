#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Core/Graphics/Font.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <utility>

// End-to-end coverage for the text-rendering pipeline: a real TTF file on
// disk, baked through Font::Load, its glyph atlas uploaded via
// SDLRenderer::CreateTexture, and drawn through DrawString against a real
// (headless) SDL renderer -- mirroring TextureRenderingIntegrationTests.cpp,
// but for text. Unit-level pieces (baking correctness, DrawString's glyph
// lookup/skip behavior) are covered by FontTests.cpp and
// SDLRendererTextureTests.cpp's DrawStringTest respectively; this test only
// cares that the pieces work when wired together, same as its texture
// counterpart.
namespace
{

using asge::graphics::Font;
using asge::graphics::RGBA_Color;
using asge::video::SDLRenderer;

std::filesystem::path AhemPath()
{
    // See tests/support/fonts/NOTICE.md.
    return std::filesystem::path(ASGE_TEST_FONTS_DIR) / "Ahem.ttf";
}

class TextRenderingIntegrationTest : public asge::test::SDLHeadlessTest
{
};

TEST_F(TextRenderingIntegrationTest, FullPipelineFromDiskToRenderedGlyphProducesNoErrors)
{
    auto fontResult = Font::Load(AhemPath(), 16);
    ASSERT_TRUE(fontResult.IsOk());
    Font font = std::move(fontResult).Value();

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    asge::test::CapturedStdout capture;

    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_NE(atlasTexture, nullptr);
    ASSERT_TRUE(atlasTexture->IsValid());

    auto glyphResult = font.GetGlyph(U'A');
    ASSERT_TRUE(glyphResult.IsOk());
    auto const& glyph = glyphResult.Value();

    asge::math::Float2 const pen{10.0f, 40.0f};
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawString("A", font, *atlasTexture, pen, RGBA_Color{0, 200, 0, 255});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty())
        << "expected no LogError output from a fully valid pipeline, got: " << capture.Str();

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

TEST_F(TextRenderingIntegrationTest, DrawsAWholeWordAcrossMultipleGlyphs)
{
    auto fontResult = Font::Load(AhemPath(), 8); // small enough that "TEST" fits in the 64x64 window
    ASSERT_TRUE(fontResult.IsOk());
    Font font = std::move(fontResult).Value();

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto atlasTexture = renderer.CreateTexture(font.GetAtlasImage());
    ASSERT_TRUE(atlasTexture->IsValid());

    asge::test::CapturedStdout capture;
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawString("TEST", font, *atlasTexture, asge::math::Float2{4.0f, 12.0f}, RGBA_Color{255, 255, 255, 255});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty()) << capture.Str();

    // Every baked Ahem glyph is a solid square filling its advance width, so
    // sampling the midpoint of each of the 4 glyphs' advance cells should
    // land inside a drawn glyph -- a cheap way to confirm all 4 characters
    // actually rendered, not just the first.
    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(m_Window), nullptr);
    ASSERT_NE(surface, nullptr);

    for (int i = 0; i < 4; ++i)
    {
        int const sampleX = 4 + i * 8 + 4; // glyph i's cell center, advance == pixel height == 8
        int const sampleY = 12 - 4;        // comfortably inside the glyph's vertical extent

        Uint8 r, g, b, a;
        ASSERT_TRUE(SDL_ReadSurfacePixel(surface, sampleX, sampleY, &r, &g, &b, &a));
        EXPECT_GT(r, 200) << "glyph " << i << " at x=" << sampleX << " did not render";
    }

    SDL_DestroySurface(surface);
}

} // namespace
