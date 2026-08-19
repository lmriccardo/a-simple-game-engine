#include "SDLHeadlessFixture.hpp"

#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// End-to-end coverage for the texture pipeline: a real BMP file on disk,
// decoded through Image::Load, uploaded via SDLRenderer::CreateTexture, and
// drawn through every IRenderer texture variant against a real (headless)
// SDL renderer -- the automated version of what examples/texture_demo shows
// a human by eye. Unit-level pieces (decode correctness, SDLTexture/
// SDLRenderer in isolation) are covered by ImageTests.cpp and
// SDLTextureTests.cpp/SDLRendererTextureTests.cpp respectively; this test
// only cares that the pieces work when wired together.
namespace
{

using asge::graphics::Image;
using asge::graphics::RGBA_Color;
using asge::video::SDLRenderer;

// A hand-built, minimal 2x2 solid-blue 24bpp BMP -- same construction as
// ImageTests.cpp's MakeSolidRedBmp. Kept local and deliberately tiny: this
// test isn't re-verifying decode correctness, just that a real file on disk
// makes it all the way to rendered pixels.
std::vector<std::byte> MakeSolidBlueBmp()
{
    auto push_u16 = [](std::vector<std::byte>& out, std::uint16_t v) {
        out.push_back(static_cast<std::byte>(v & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
    };
    auto push_u32 = [](std::vector<std::byte>& out, std::uint32_t v) {
        out.push_back(static_cast<std::byte>(v & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 16) & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 24) & 0xFF));
    };

    constexpr std::uint32_t kPixelDataSize = 16; // 2 rows * 8 bytes (6 data + 2 padding)
    constexpr std::uint32_t kFileSize = 54 + kPixelDataSize;

    std::vector<std::byte> bmp;
    bmp.reserve(kFileSize);

    // BITMAPFILEHEADER (14 bytes)
    bmp.push_back(std::byte{'B'});
    bmp.push_back(std::byte{'M'});
    push_u32(bmp, kFileSize);
    push_u32(bmp, 0);  // reserved
    push_u32(bmp, 54); // pixel data offset

    // BITMAPINFOHEADER (40 bytes)
    push_u32(bmp, 40); // header size
    push_u32(bmp, 2);  // width
    push_u32(bmp, 2);  // height (positive = bottom-up source rows)
    push_u16(bmp, 1);  // planes
    push_u16(bmp, 24); // bits per pixel
    push_u32(bmp, 0);  // compression = BI_RGB
    push_u32(bmp, kPixelDataSize);
    push_u32(bmp, 0); // x pixels per meter
    push_u32(bmp, 0); // y pixels per meter
    push_u32(bmp, 0); // colors used
    push_u32(bmp, 0); // important colors

    // Pixel data: 2 rows, each "B G R B G R <2 bytes padding>" for solid blue
    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 2; ++col)
        {
            bmp.push_back(std::byte{0xFF}); // B
            bmp.push_back(std::byte{0x00}); // G
            bmp.push_back(std::byte{0x00}); // R
        }
        bmp.push_back(std::byte{0x00}); // row padding to a multiple of 4 bytes
        bmp.push_back(std::byte{0x00});
    }

    return bmp;
}

class TextureRenderingIntegrationTest : public asge::test::SDLHeadlessTest
{
protected:
    std::filesystem::path m_ImagePath;

    void SetUp() override
    {
        SDLHeadlessTest::SetUp();

        auto const uniqueName = "asge_texture_integration_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".bmp";
        m_ImagePath = std::filesystem::temp_directory_path() / uniqueName;

        auto const bmp = MakeSolidBlueBmp();
        std::ofstream file(m_ImagePath, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<char const*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_ImagePath, ec);
        SDLHeadlessTest::TearDown();
    }
};

TEST_F(TextureRenderingIntegrationTest, FullPipelineFromDiskToRenderedPixelsProducesNoErrors)
{
    auto imageResult = Image::Load(m_ImagePath);
    ASSERT_TRUE(imageResult.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    asge::test::CapturedStdout capture;

    auto texture = renderer.CreateTexture(imageResult.Value());
    ASSERT_NE(texture, nullptr);
    ASSERT_TRUE(texture->IsValid());

    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 64.0f, 64.0f});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty())
        << "expected no LogError output from a fully valid pipeline, got: " << capture.Str();

    SDL_Surface* surface = SDL_RenderReadPixels(SDL_GetRenderer(m_Window), nullptr);
    ASSERT_NE(surface, nullptr);

    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(surface, 32, 32, &r, &g, &b, &a));
    EXPECT_LT(r, 50);
    EXPECT_LT(g, 50);
    EXPECT_GT(b, 200);

    SDL_DestroySurface(surface);
}

TEST_F(TextureRenderingIntegrationTest, EveryDrawTextureVariantRunsAgainstARealDecodedTexture)
{
    auto imageResult = Image::Load(m_ImagePath);
    ASSERT_TRUE(imageResult.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto texture = renderer.CreateTexture(imageResult.Value());
    ASSERT_TRUE(texture->IsValid());

    asge::test::CapturedStdout capture;

    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 16.0f, 16.0f});
    renderer.DrawTexture(*texture, asge::math::Float2{20.0f, 0.0f});
    renderer.DrawTexture9Grid(*texture, 1.0f, 1.0f, 1.0f, 1.0f, asge::math::Rect{0.0f, 20.0f, 16.0f, 16.0f});
    renderer.DrawTextureTiled(*texture, 1.0f, asge::math::Rect{20.0f, 20.0f, 16.0f, 16.0f});
    renderer.DrawTextureAffine(*texture,
        asge::math::Float2{0.0f, 40.0f}, asge::math::Float2{16.0f, 40.0f}, asge::math::Float2{0.0f, 56.0f});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty())
        << "expected no LogError output, got: " << capture.Str();
}

// Mirrors texture_demo's LoadTexture(): a failed Image::Load is expected to be
// handled by the caller (LogError + nullptr, no CreateTexture call) rather
// than left to corrupt renderer state for whatever gets drawn afterward.
TEST_F(TextureRenderingIntegrationTest, MissingAssetLeavesRendererUsableForSubsequentDraws)
{
    auto missingPath = m_ImagePath;
    missingPath += ".missing";
    auto missingResult = Image::Load(missingPath);
    ASSERT_FALSE(missingResult.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto goodResult = Image::Load(m_ImagePath);
    ASSERT_TRUE(goodResult.IsOk());
    auto texture = renderer.CreateTexture(goodResult.Value());
    ASSERT_TRUE(texture->IsValid());

    asge::test::CapturedStdout capture;
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 64.0f, 64.0f});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty()) << capture.Str();
}

} // namespace
