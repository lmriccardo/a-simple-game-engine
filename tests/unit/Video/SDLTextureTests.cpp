#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLTexture.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <utility>

// These tests exercise the real SDL-backed SDLTexture against SDL's dummy
// video driver + software renderer (see SDLHeadlessFixture.hpp) -- unlike
// GraphicsInterfaceTests.cpp, which only proves the ITexture contract shape
// through a hand-written fake, this is the actual SDL_CreateTexture /
// SDL_UpdateTexture code path.
namespace
{

using asge::graphics::Image;
using asge::graphics::PixelFormat;
using asge::graphics::RGBA_Color;
using asge::video::SDLTexture;

Image MakeImage(std::size_t inW, std::size_t inH, std::uint8_t inFill = 128)
{
    Image::data_t pixels(inW * inH * 4, inFill);
    return Image(inW, inH, PixelFormat::RGBA8, pixels);
}

Image MakeA8Image(std::size_t inW, std::size_t inH, std::uint8_t inFill)
{
    Image::data_t pixels(inW * inH, inFill);
    return Image(inW, inH, PixelFormat::A8, pixels);
}

// SDLTexture's constructor takes a raw SDL_Renderer*, not IRenderer -- these
// tests need that raw handle directly, so this fixture creates one itself
// rather than going through SDLRenderer.
class SDLTextureTest : public asge::test::SDLHeadlessTest
{
protected:
    SDL_Renderer* m_Renderer{nullptr};

    void SetUp() override
    {
        SDLHeadlessTest::SetUp();
        m_Renderer = SDL_CreateRenderer(m_Window, "software");
        ASSERT_NE(m_Renderer, nullptr) << SDL_GetError();
    }

    void TearDown() override
    {
        if (m_Renderer) SDL_DestroyRenderer(m_Renderer);
        SDLHeadlessTest::TearDown();
    }
};

TEST_F(SDLTextureTest, ValidImageProducesUsableTexture)
{
    auto image = MakeImage(4, 4);
    SDLTexture texture(m_Renderer, image);

    EXPECT_TRUE(texture.IsValid());
    EXPECT_EQ(texture.Size().x(), 4);
    EXPECT_EQ(texture.Size().y(), 4);
    EXPECT_NE(texture.NativeHandle(), nullptr);
}

TEST_F(SDLTextureTest, ZeroSizedImageFailsCreationAndLogsTextureCreationFailed)
{
    auto image = MakeImage(0, 0);

    asge::test::CapturedStdout capture;
    SDLTexture texture(m_Renderer, image);
    auto const output = capture.Str();

    EXPECT_FALSE(texture.IsValid());
    EXPECT_EQ(texture.NativeHandle(), nullptr);
    EXPECT_NE(output.find("failed to create the texture"), std::string::npos) << output;
}

TEST_F(SDLTextureTest, SizeOnDefaultConstructedHandleDoesNotCrash)
{
    // Move-from state: no SDL handle at all. Size() used to dereference
    // m_Handle unconditionally -- guards against that regression.
    auto image = MakeImage(0, 0); // guaranteed-invalid handle, see test above
    SDLTexture texture(m_Renderer, image);

    ASSERT_FALSE(texture.IsValid());
    EXPECT_EQ(texture.Size().x(), 0);
    EXPECT_EQ(texture.Size().y(), 0);
}

TEST_F(SDLTextureTest, MoveConstructionLeavesSourceInert)
{
    auto image = MakeImage(2, 2);
    SDLTexture original(m_Renderer, image);
    ASSERT_TRUE(original.IsValid());

    SDLTexture moved(std::move(original));

    EXPECT_TRUE(moved.IsValid());
    EXPECT_EQ(original.NativeHandle(), nullptr);
}

TEST_F(SDLTextureTest, A8ImageProducesUsableTexture)
{
    // SDL3 rejects palettized textures outright (SDL_PIXELFORMAT_INDEX8 ->
    // "Palettized textures are not supported", confirmed empirically), so
    // A8 images upload as RGBA32 via SDLPixelFormat's expansion. Construction
    // succeeding at all is the first proof that path works.
    auto image = MakeA8Image(4, 4, 200);
    SDLTexture texture(m_Renderer, image);

    EXPECT_TRUE(texture.IsValid());
    EXPECT_EQ(texture.Size().x(), 4);
    EXPECT_EQ(texture.Size().y(), 4);
}

TEST_F(SDLTextureTest, A8ImageRendersAsWhiteMaskedByItsAlphaByte)
{
    // End-to-end proof, not just construction: a fully-opaque (255) A8 pixel
    // should composite as opaque white over a black background, which is
    // only true if ExpandPixelsForUpload actually placed the source byte in
    // the alpha channel (RGB=255,255,255) rather than, say, misreading raw
    // single-channel bytes as packed RGBA garbage.
    auto image = MakeA8Image(2, 2, 255);
    SDLTexture texture(m_Renderer, image);
    ASSERT_TRUE(texture.IsValid());

    ASSERT_TRUE(SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255));
    ASSERT_TRUE(SDL_RenderClear(m_Renderer));

    auto* sdlTexture = static_cast<SDL_Texture*>(texture.NativeHandle());
    SDL_FRect dst{0.0f, 0.0f, 8.0f, 8.0f};
    ASSERT_TRUE(SDL_RenderTexture(m_Renderer, sdlTexture, nullptr, &dst));
    ASSERT_TRUE(SDL_RenderPresent(m_Renderer));

    SDL_Surface* surface = SDL_RenderReadPixels(m_Renderer, nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 4, 4, &r, &g, &b, &a));
    EXPECT_GT(r, 200);
    EXPECT_GT(g, 200);
    EXPECT_GT(b, 200);

    SDL_DestroySurface(surface);
}

TEST_F(SDLTextureTest, GetColorModRoundTripsWhatSetColorModWrote)
{
    auto image = MakeImage(2, 2);
    SDLTexture texture(m_Renderer, image);
    ASSERT_TRUE(texture.IsValid());

    texture.SetColorMod(RGBA_Color{200, 100, 50, 25});

    auto modResult = texture.GetColorMod();
    ASSERT_TRUE(modResult.IsOk());
    EXPECT_EQ(modResult.Value().r, 200);
    EXPECT_EQ(modResult.Value().g, 100);
    EXPECT_EQ(modResult.Value().b, 50);
    EXPECT_EQ(modResult.Value().a, 25);
}

TEST_F(SDLTextureTest, SetColorModTintsSubsequentlyRenderedPixels)
{
    // End-to-end proof for DrawText's whole approach: it tints a shared
    // white/alpha-only atlas via SetColorMod rather than baking color into
    // the texture itself. A white source pixel modulated by (255,0,0)
    // should render as pure red, not white.
    auto image = MakeImage(2, 2, 255); // opaque white
    SDLTexture texture(m_Renderer, image);
    ASSERT_TRUE(texture.IsValid());
    texture.SetColorMod(RGBA_Color{255, 0, 0, 255});

    ASSERT_TRUE(SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255));
    ASSERT_TRUE(SDL_RenderClear(m_Renderer));

    auto* sdlTexture = static_cast<SDL_Texture*>(texture.NativeHandle());
    SDL_FRect dst{0.0f, 0.0f, 8.0f, 8.0f};
    ASSERT_TRUE(SDL_RenderTexture(m_Renderer, sdlTexture, nullptr, &dst));
    ASSERT_TRUE(SDL_RenderPresent(m_Renderer));

    SDL_Surface* surface = SDL_RenderReadPixels(m_Renderer, nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 4, 4, &r, &g, &b, &a));
    EXPECT_GT(r, 200);
    EXPECT_LT(g, 50);
    EXPECT_LT(b, 50);

    SDL_DestroySurface(surface);
}

TEST_F(SDLTextureTest, MoveAssignmentTransfersHandleAndLeavesSourceInert)
{
    auto imageA = MakeImage(2, 2);
    auto imageB = MakeImage(3, 3);
    SDLTexture a(m_Renderer, imageA);
    SDLTexture b(m_Renderer, imageB);
    ASSERT_TRUE(a.IsValid());
    ASSERT_TRUE(b.IsValid());

    a = std::move(b);

    EXPECT_TRUE(a.IsValid());
    EXPECT_EQ(a.Size().x(), 3);
    EXPECT_EQ(b.NativeHandle(), nullptr);
}

} // namespace
