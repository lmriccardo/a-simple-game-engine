#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// A hand-built, minimal 2x2 solid-red 24bpp BMP -- same fixture as
// ImageTests.cpp's MakeSolidRedBmp, duplicated locally so this suite doesn't
// need to reach into another test target's translation unit.
namespace
{

using namespace asge::game::asset;
using asge::errors::VfsError;
using asge::errors::ImageError;

std::vector<std::byte> MakeSolidRedBmp()
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
    push_u32(bmp, 0); // reserved
    push_u32(bmp, 54); // pixel data offset

    // BITMAPINFOHEADER (40 bytes)
    push_u32(bmp, 40); // header size
    push_u32(bmp, 2);  // width
    push_u32(bmp, 2);  // height (positive = bottom-up source rows)
    push_u16(bmp, 1);  // planes
    push_u16(bmp, 24); // bits per pixel
    push_u32(bmp, 0);  // compression = BI_RGB
    push_u32(bmp, kPixelDataSize);
    push_u32(bmp, 0);  // x pixels per meter
    push_u32(bmp, 0);  // y pixels per meter
    push_u32(bmp, 0);  // colors used
    push_u32(bmp, 0);  // important colors

    // Pixel data: 2 rows, each "B G R B G R <2 bytes padding>" for solid red
    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 2; ++col)
        {
            bmp.push_back(std::byte{0x00}); // B
            bmp.push_back(std::byte{0x00}); // G
            bmp.push_back(std::byte{0xFF}); // R
        }
        bmp.push_back(std::byte{0x00}); // row padding to a multiple of 4 bytes
        bmp.push_back(std::byte{0x00});
    }

    return bmp;
}

class AssetManagerTest : public ::testing::Test
{
protected:
    std::filesystem::path m_ImagesDir;
    asge::filesystem::VirtualFileSystem m_Vfs;

    void SetUp() override
    {
        auto const uniqueName = "asge_assetmanager_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_ImagesDir = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(m_ImagesDir);

        WriteBytes(m_ImagesDir / "hero.bmp", MakeSolidRedBmp());
        WriteText(m_ImagesDir / "garbage.bmp", "not a bmp");

        ASSERT_TRUE(m_Vfs.Mount("images", m_ImagesDir.string()).IsOk());
        ASSERT_TRUE(m_Vfs.Mount("fonts", std::string(ASGE_TEST_FONTS_DIR)).IsOk());
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_ImagesDir, ec);
    }

    static void WriteBytes(std::filesystem::path const& inPath, std::vector<std::byte> const& inBytes)
    {
        std::ofstream file(inPath, std::ios::trunc | std::ios::binary);
        file.write(reinterpret_cast<char const*>(inBytes.data()),
            static_cast<std::streamsize>(inBytes.size()));
    }

    static void WriteText(std::filesystem::path const& inPath, std::string const& inContent)
    {
        std::ofstream file(inPath, std::ios::trunc | std::ios::binary);
        file << inContent;
    }
};

// ─── LoadImage ──────────────────────────────────────────────────────────────────

TEST_F(AssetManagerTest, LoadImage_ValidPathReturnsDecodedImage)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.LoadImage("images/hero.bmp");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value()->Get().Dimensions().x(), 2);
    EXPECT_EQ(result.Value()->Get().Dimensions().y(), 2);
    EXPECT_EQ(result.Value()->VirtualPath(), "images/hero.bmp");
}

TEST_F(AssetManagerTest, LoadImage_UnresolvableVirtualPathReturnsNotMountedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.LoadImage("images/missing.bmp");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(AssetManagerTest, LoadImage_GarbageBytesReturnsDecodeFailedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.LoadImage("images/garbage.bmp");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ImageError::DecodeFailed));
}

TEST_F(AssetManagerTest, LoadImage_SamePathTwiceReturnsSameCachedAsset)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.LoadImage("images/hero.bmp");
    auto second = mgr.LoadImage("images/hero.bmp");
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
}

// ─── LoadFont ───────────────────────────────────────────────────────────────────

TEST_F(AssetManagerTest, LoadFont_ValidPathReturnsBakedFont)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.LoadFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(result.IsOk());
    EXPECT_GT(result.Value()->Get().GetLineHeight(), 0);
    EXPECT_EQ(result.Value()->VirtualPath(), "fonts/Ahem.ttf");
}

TEST_F(AssetManagerTest, LoadFont_NonExistentPathReturnsNotMountedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.LoadFont("fonts/DoesNotExist.ttf", 32);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(AssetManagerTest, LoadFont_DifferentPixelHeightsAreSeparateCacheEntries)
{
    AssetManager mgr(m_Vfs);

    auto small = mgr.LoadFont("fonts/Ahem.ttf", 16);
    auto large = mgr.LoadFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(small.IsOk());
    ASSERT_TRUE(large.IsOk());
    EXPECT_NE(small.Value(), large.Value());
}

TEST_F(AssetManagerTest, LoadFont_SamePathAndHeightTwiceReturnsSameCachedAsset)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.LoadFont("fonts/Ahem.ttf", 32);
    auto second = mgr.LoadFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
}

}
