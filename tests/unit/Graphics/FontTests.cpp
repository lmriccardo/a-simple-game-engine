#include <ASGE/Core/Graphics/Font.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

// Font::Load's success path -- baking, glyph metrics, the atlas image --
// needs a real, valid TTF; unlike the hand-built BMP fixtures used
// elsewhere in this suite, TrueType's table-directory + checksum format
// isn't practical to hand-construct. Ahem.ttf (tests/support/fonts/, see
// NOTICE.md) is a small, public-domain font the W3C maintains specifically
// for automated rendering tests: every glyph is a solid square filling its
// advance width, which is also what makes the pixel-readback assertions in
// SDLRendererTextureTests/TextRenderingIntegrationTests possible.
namespace
{

using namespace asge::graphics;
using asge::errors::FontError;

std::filesystem::path AhemPath()
{
    return std::filesystem::path(ASGE_TEST_FONTS_DIR) / "Ahem.ttf";
}

TEST(FontLoadTest, NonExistentPathReturnsError)
{
    auto result = Font::Load(AhemPath().replace_extension(".missing"), 32);
    EXPECT_FALSE(result.IsOk());
}

class FontLoadGarbageTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Path;

    void SetUp() override
    {
        auto const uniqueName = "asge_font_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".ttf";
        m_Path = std::filesystem::temp_directory_path() / uniqueName;

        std::ofstream file(m_Path, std::ios::binary | std::ios::trunc);
        file << "not a font";
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_Path, ec);
    }
};

TEST_F(FontLoadGarbageTest, GarbageBytesReturnInitFailedError)
{
    auto result = Font::Load(m_Path, 32);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(FontError::InitFailed));
}

TEST(FontLoadTest, ValidTTFBakesWithSaneVerticalMetrics)
{
    auto result = Font::Load(AhemPath(), 32);
    ASSERT_TRUE(result.IsOk());

    auto const& font = result.Value();
    EXPECT_GT(font.GetLineHeight(), 0);
    EXPECT_GT(font.GetAscent(), 0);
    EXPECT_LE(font.GetDescent(), 0);
}

TEST(FontLoadTest, BakedAtlasIsA8)
{
    auto result = Font::Load(AhemPath(), 32);
    ASSERT_TRUE(result.IsOk());

    auto const& atlas = result.Value().GetAtlasImage();
    EXPECT_EQ(atlas.Format(), PixelFormat::A8);
    EXPECT_GT(atlas.Dimensions().x(), 0);
    EXPECT_GT(atlas.Dimensions().y(), 0);
}

TEST(FontGlyphTest, BakedCodepointAdvanceMatchesRequestedPixelHeight)
{
    // Ahem's defining property: at a given bake height, every glyph's
    // advance equals that height exactly -- the whole reason it's usable
    // for pixel-predictable rendering assertions.
    constexpr int pixelHeight = 32;
    auto result = Font::Load(AhemPath(), pixelHeight);
    ASSERT_TRUE(result.IsOk());

    auto glyph = result.Value().GetGlyph(U'A');
    ASSERT_TRUE(glyph.IsOk());
    EXPECT_EQ(glyph.Value().advance, pixelHeight);
}

TEST(FontGlyphTest, UnbakedCodepointReturnsUnexistingCodepointError)
{
    auto result = Font::Load(AhemPath(), 32);
    ASSERT_TRUE(result.IsOk());

    // U+00A9 (copyright sign) is well outside the baked ASCII 32-126 range.
    auto glyph = result.Value().GetGlyph(static_cast<char32_t>(0x00A9));

    ASSERT_FALSE(glyph.IsOk());
    EXPECT_EQ(glyph.Code(), make_error_code(FontError::UnexistingCodepoint));
}

TEST(FontMoveTest, MoveConstructionPreservesGlyphLookupAndAtlas)
{
    auto result = Font::Load(AhemPath(), 32);
    ASSERT_TRUE(result.IsOk());

    Font moved(std::move(result).Value());

    EXPECT_TRUE(moved.GetGlyph(U'A').IsOk());
    EXPECT_GT(moved.GetAtlasImage().Dimensions().x(), 0);
}

} // namespace
