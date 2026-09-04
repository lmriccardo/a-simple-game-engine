#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Media/PixelFormat.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{

using namespace asge::media;
using asge::errors::ImageError;

// ─── A hand-built, minimal 2x2 solid-red 24bpp BMP ─────────────────────────────
// stb_image always returns pixel rows top-to-bottom regardless of the source
// format's own row order, so a solid color keeps this fixture simple while
// still exercising the real decode path (header parsing, BGR->RGBA, padding).
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

std::vector<std::byte> MakeGarbageBytes()
{
    return { std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03} };
}

void ExpectDecodedSolidRedImage(Image const& inImage)
{
    EXPECT_EQ(inImage.Dimensions().x(), 2);
    EXPECT_EQ(inImage.Dimensions().y(), 2);
    EXPECT_EQ(inImage.Format(), PixelFormat::RGBA8);
    EXPECT_EQ(inImage.Stride(), 8u); // 2 pixels * 4 bytes

    auto const* pixels = inImage.Data();
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 255); // R
    EXPECT_EQ(pixels[1], 0);   // G
    EXPECT_EQ(pixels[2], 0);   // B
    EXPECT_EQ(pixels[3], 255); // A
}

TEST(PixelFormatTest, RGBA8_IsFourBytesPerPixel)
{
    EXPECT_EQ(PixelFormatInfoFor(PixelFormat::RGBA8).s_BytesPerPixel, 4u);
}

TEST(PixelFormatTest, A8_IsOneBytePerPixel)
{
    EXPECT_EQ(PixelFormatInfoFor(PixelFormat::A8).s_BytesPerPixel, 1u);
}

TEST(ImageTest, Stride_MatchesWidthTimesBytesPerPixel)
{
    Image::data_t data(3 * 2 * 4, std::uint8_t{0});
    Image image(3, 2, PixelFormat::RGBA8, data);

    EXPECT_EQ(image.Stride(), 12u);
    EXPECT_EQ(image.Dimensions().x(), 3);
    EXPECT_EQ(image.Dimensions().y(), 2);
}

TEST(ImageTest, Stride_UsesFormatSpecificBytesPerPixel_ForA8)
{
    // Guards Stride() actually consulting PixelFormatInfoFor per-format,
    // rather than assuming RGBA8's 4 bytes/pixel for every format.
    Image::data_t data(3 * 2 * 1, std::uint8_t{0});
    Image image(3, 2, PixelFormat::A8, data);

    EXPECT_EQ(image.Stride(), 3u);
}

TEST(DecodeImageTest, InvalidBytesReturnsDecodeFailedError)
{
    auto result = DecodeImage(MakeGarbageBytes());

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ImageError::DecodeFailed));
}

TEST(DecodeImageTest, ValidBmpBytesDecodeToRGBA8)
{
    auto result = DecodeImage(MakeSolidRedBmp());

    ASSERT_TRUE(result.IsOk());
    ExpectDecodedSolidRedImage(result.Value());
}

class ReadImageTest : public ::testing::Test
{
protected:
    std::filesystem::path m_ImagePath;

    void SetUp() override
    {
        auto const uniqueName = "asge_image_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".bmp";
        m_ImagePath = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_ImagePath, ec);
    }

    void WriteSolidRedBmp() const
    {
        auto const bmp = MakeSolidRedBmp();
        std::ofstream file(m_ImagePath, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<char const*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
    }
};

TEST_F(ReadImageTest, NonExistentPathReturnsError)
{
    auto result = Image::Load(m_ImagePath);
    EXPECT_FALSE(result.IsOk());
}

TEST_F(ReadImageTest, ValidBmpFileDecodesSuccessfully)
{
    WriteSolidRedBmp();

    auto result = Image::Load(m_ImagePath);

    ASSERT_TRUE(result.IsOk());
    ExpectDecodedSolidRedImage(result.Value());
}

// ─── AlphaContentBounds ─────────────────────────────────────────────────────

// Builds an RGBA8 image where the pixel at (x, y) is opaque iff inGetAlpha(x, y)
// is true, fully transparent otherwise. RGB is arbitrary, non-zero, so a test
// accidentally checking color bytes instead of alpha would still fail loudly.
template<typename AlphaFn>
Image MakeRgba8Image(std::size_t inWidth, std::size_t inHeight, AlphaFn inGetAlpha)
{
    Image::data_t data(inWidth * inHeight * 4, std::uint8_t{0});
    for (std::size_t y = 0; y < inHeight; ++y)
    {
        for (std::size_t x = 0; x < inWidth; ++x)
        {
            std::size_t const idx = (y * inWidth + x) * 4;
            data[idx + 0] = 200;
            data[idx + 1] = 100;
            data[idx + 2] = 50;
            data[idx + 3] = inGetAlpha(x, y) ? std::uint8_t{255} : std::uint8_t{0};
        }
    }
    return Image(inWidth, inHeight, PixelFormat::RGBA8, data);
}

// Same as MakeRgba8Image, but for A8 -- the pixel byte itself is the alpha.
template<typename AlphaFn>
Image MakeA8Image(std::size_t inWidth, std::size_t inHeight, AlphaFn inGetAlpha)
{
    Image::data_t data(inWidth * inHeight, std::uint8_t{0});
    for (std::size_t y = 0; y < inHeight; ++y)
        for (std::size_t x = 0; x < inWidth; ++x)
            data[y * inWidth + x] = inGetAlpha(x, y) ? std::uint8_t{255} : std::uint8_t{0};
    return Image(inWidth, inHeight, PixelFormat::A8, data);
}

TEST(AlphaContentBoundsTest, FullyOpaqueImage_ReturnsWholeImage)
{
    Image image = MakeRgba8Image(4, 3, [](std::size_t, std::size_t) { return true; });

    auto const bounds = image.AlphaContentBounds();

    EXPECT_FLOAT_EQ(bounds.x, 0.0f);
    EXPECT_FLOAT_EQ(bounds.y, 0.0f);
    EXPECT_FLOAT_EQ(bounds.w, 4.0f);
    EXPECT_FLOAT_EQ(bounds.h, 3.0f);
}

TEST(AlphaContentBoundsTest, PaddedSprite_ReturnsTightBoundsSmallerThanCanvas)
{
    // 6x6 canvas, only the 2x2 block at (2,2)-(3,3) is opaque -- the rest is
    // the kind of mixed padding issue #46 was filed about.
    Image image = MakeRgba8Image(6, 6, [](std::size_t x, std::size_t y) {
        return x >= 2 && x <= 3 && y >= 2 && y <= 3;
    });

    auto const bounds = image.AlphaContentBounds();

    EXPECT_FLOAT_EQ(bounds.x, 2.0f);
    EXPECT_FLOAT_EQ(bounds.y, 2.0f);
    EXPECT_FLOAT_EQ(bounds.w, 2.0f);
    EXPECT_FLOAT_EQ(bounds.h, 2.0f);
}

TEST(AlphaContentBoundsTest, RegionOverload_OnlyConsidersPixelsInsideRegion)
{
    // Opaque pixel at (0,0) sits outside the scanned region and must be
    // ignored; only (5,5), inside the region, should shape the result.
    Image image = MakeRgba8Image(10, 10, [](std::size_t x, std::size_t y) {
        return (x == 0 && y == 0) || (x == 5 && y == 5);
    });

    auto const bounds = image.AlphaContentBounds(asge::math::Rect{ 4.0f, 4.0f, 4.0f, 4.0f });

    EXPECT_FLOAT_EQ(bounds.x, 5.0f);
    EXPECT_FLOAT_EQ(bounds.y, 5.0f);
    EXPECT_FLOAT_EQ(bounds.w, 1.0f);
    EXPECT_FLOAT_EQ(bounds.h, 1.0f);
}

TEST(AlphaContentBoundsTest, RegionExtendingPastImageBounds_IsClamped)
{
    Image image = MakeRgba8Image(4, 4, [](std::size_t, std::size_t) { return true; });

    auto const bounds = image.AlphaContentBounds(asge::math::Rect{ 2.0f, 2.0f, 100.0f, 100.0f });

    EXPECT_FLOAT_EQ(bounds.x, 2.0f);
    EXPECT_FLOAT_EQ(bounds.y, 2.0f);
    EXPECT_FLOAT_EQ(bounds.w, 2.0f); // clamped: image width(4) - region x(2)
    EXPECT_FLOAT_EQ(bounds.h, 2.0f);
}

TEST(AlphaContentBoundsTest, EntirelyTransparentRegion_FallsBackToClampedRegion)
{
    Image image = MakeRgba8Image(10, 10, [](std::size_t, std::size_t) { return false; });

    auto const bounds = image.AlphaContentBounds(asge::math::Rect{ 2.0f, 3.0f, 5.0f, 100.0f });

    // Falls back to the region itself, but still clamped -- h shrinks from
    // 100 to what actually fits (image height 10 - region y 3 = 7).
    EXPECT_FLOAT_EQ(bounds.x, 2.0f);
    EXPECT_FLOAT_EQ(bounds.y, 3.0f);
    EXPECT_FLOAT_EQ(bounds.w, 5.0f);
    EXPECT_FLOAT_EQ(bounds.h, 7.0f);
}

TEST(AlphaContentBoundsTest, A8Format_TreatsThePixelByteItselfAsAlpha)
{
    Image image = MakeA8Image(5, 5, [](std::size_t x, std::size_t y) {
        return x == 1 && y == 1;
    });

    auto const bounds = image.AlphaContentBounds();

    EXPECT_FLOAT_EQ(bounds.x, 1.0f);
    EXPECT_FLOAT_EQ(bounds.y, 1.0f);
    EXPECT_FLOAT_EQ(bounds.w, 1.0f);
    EXPECT_FLOAT_EQ(bounds.h, 1.0f);
}

}
