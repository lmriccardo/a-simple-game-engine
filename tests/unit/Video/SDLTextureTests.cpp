#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLTexture.hpp>
#include <ASGE/Core/Graphics/Image.hpp>

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
using asge::video::SDLTexture;

Image MakeImage(std::size_t inW, std::size_t inH, std::uint8_t inFill = 128)
{
    Image::data_t pixels(inW * inH * 4, inFill);
    return Image(inW, inH, PixelFormat::RGBA8, pixels);
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
