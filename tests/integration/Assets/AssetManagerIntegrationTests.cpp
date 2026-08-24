#include "SDLHeadlessFixture.hpp"

#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Video/Graphics/Rendering/SDL/SDLRenderer.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// End-to-end coverage for the asset pipeline: a real BMP/TTF on disk, mounted
// into a VirtualFileSystem, loaded by virtual path through AssetManager, and
// the resulting Asset<Image>/Asset<Font> uploaded and drawn against a real
// (headless) SDL renderer -- mirroring TextureRenderingIntegrationTests.cpp/
// TextRenderingIntegrationTests.cpp, but sourced through the asset pipeline
// (VFS resolve + AssetPool caching) instead of calling Image::Load/Font::Load
// directly. Unit-level pieces (VFS resolution, AssetPool caching, decode/bake
// correctness) are covered by VirtualFileSystemTests.cpp, AssetPoolTests.cpp/
// AssetManagerTests.cpp, and ImageTests.cpp/FontTests.cpp respectively; this
// test only cares that the pieces work when wired together.
namespace
{

using asge::game::asset::AssetManager;
using asge::graphics::RGBA_Color;
using asge::video::SDLRenderer;

// A hand-built, minimal 2x2 solid-blue 24bpp BMP -- same construction as
// TextureRenderingIntegrationTests.cpp's MakeSolidBlueBmp.
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

class AssetManagerIntegrationTest : public asge::test::SDLHeadlessTest
{
protected:
    std::filesystem::path m_ImagesDir;
    asge::filesystem::VirtualFileSystem m_Vfs;

    void SetUp() override
    {
        SDLHeadlessTest::SetUp();

        auto const uniqueName = "asge_asset_integration_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_ImagesDir = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(m_ImagesDir);

        auto const bmp = MakeSolidBlueBmp();
        std::ofstream file(m_ImagesDir / "hero.bmp", std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<char const*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
        file.close();

        ASSERT_TRUE(m_Vfs.Mount("images", m_ImagesDir.string()).IsOk());
        // See tests/support/fonts/NOTICE.md.
        ASSERT_TRUE(m_Vfs.Mount("fonts", std::string(ASGE_TEST_FONTS_DIR)).IsOk());
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_ImagesDir, ec);
        SDLHeadlessTest::TearDown();
    }
};

TEST_F(AssetManagerIntegrationTest, FullPipelineFromVirtualPathToRenderedPixelsProducesNoErrors)
{
    AssetManager assets(m_Vfs);
    auto imageAsset = assets.LoadImage("images/hero.bmp");
    ASSERT_TRUE(imageAsset.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    asge::test::CapturedStdout capture;

    auto texture = renderer.CreateTexture(imageAsset.Value()->Get());
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

TEST_F(AssetManagerIntegrationTest, FullPipelineFromVirtualPathToRenderedGlyphProducesNoErrors)
{
    AssetManager assets(m_Vfs);
    auto fontAsset = assets.LoadFont("fonts/Ahem.ttf", 16);
    ASSERT_TRUE(fontAsset.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    asge::test::CapturedStdout capture;

    auto atlasTexture = renderer.CreateTexture(fontAsset.Value()->Get().GetAtlasImage());
    ASSERT_NE(atlasTexture, nullptr);
    ASSERT_TRUE(atlasTexture->IsValid());

    auto glyphResult = fontAsset.Value()->Get().GetGlyph(U'A');
    ASSERT_TRUE(glyphResult.IsOk());
    auto const& glyph = glyphResult.Value();

    asge::math::Float2 const pen{10.0f, 40.0f};
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawString("A", fontAsset.Value()->Get(), *atlasTexture, pen, RGBA_Color{0, 200, 0, 255});
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

// Mirrors TextureRenderingIntegrationTests.cpp's MissingAssetLeavesRenderer
// UsableForSubsequentDraws: an unresolvable virtual path is expected to be
// handled by the caller (an error Result, no texture created) rather than
// leave the AssetManager/renderer unusable for whatever loads afterward.
TEST_F(AssetManagerIntegrationTest, MissingVirtualPathLeavesAssetManagerAndRendererUsableAfterwards)
{
    AssetManager assets(m_Vfs);

    auto missing = assets.LoadImage("images/does_not_exist.bmp");
    ASSERT_FALSE(missing.IsOk());

    SDLRenderer renderer(m_Window);
    ASSERT_TRUE(renderer.IsValid());

    auto good = assets.LoadImage("images/hero.bmp");
    ASSERT_TRUE(good.IsOk());
    auto texture = renderer.CreateTexture(good.Value()->Get());
    ASSERT_TRUE(texture->IsValid());

    asge::test::CapturedStdout capture;
    renderer.Clear(RGBA_Color{0, 0, 0, 255});
    renderer.DrawTexture(*texture, asge::math::Rect{0.0f, 0.0f, 64.0f, 64.0f});
    renderer.Present();

    EXPECT_TRUE(capture.Str().empty()) << capture.Str();
}

} // namespace
