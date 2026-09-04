#include "Image.hpp"

#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{

/** @brief Clamps inRegion so it lies fully within an image of inDimensions pixels. */
asge::math::Rect ClampRegionToImage( asge::math::Rect const& inRegion, asge::math::Int2 inDimensions ) noexcept
{
    float const imgW = static_cast<float>( inDimensions.x() );
    float const imgH = static_cast<float>( inDimensions.y() );

    float const x0 = std::clamp( inRegion.x, 0.0f, imgW );
    float const y0 = std::clamp( inRegion.y, 0.0f, imgH );
    float const x1 = std::clamp( inRegion.x + inRegion.w, 0.0f, imgW );
    float const y1 = std::clamp( inRegion.y + inRegion.h, 0.0f, imgH );

    return { x0, y0, std::max( 0.0f, x1 - x0 ), std::max( 0.0f, y1 - y0 ) };
}

/** @brief True if the pixel at (inX, inY) in inImage is fully transparent (alpha == 0). */
bool IsPixelTransparent( asge::media::Image const& inImage, std::size_t inX, std::size_t inY ) noexcept
{
    // Alpha is always the last byte of a pixel here: offset 3 of 4 for RGBA8,
    // offset 0 of 1 for A8 (the pixel *is* its alpha).
    std::size_t const bpp = asge::media::PixelFormatInfoFor( inImage.Format() ).s_BytesPerPixel;
    std::uint8_t const* row = inImage.Data() + inY * inImage.Stride();
    return row[inX * bpp + (bpp - 1)] == 0;
}

}

asge::media::Image::Image(
    std::size_t inW, std::size_t inH, PixelFormat inFormat, data_t const &inData
) : m_Width(inW), m_Height(inH), m_Format(inFormat), m_Data(inData)
{}

asge::math::Int2 asge::media::Image::Dimensions() const noexcept
{
    return { static_cast<int>(m_Width), static_cast<int>(m_Height) };
}

asge::media::PixelFormat asge::media::Image::Format() const noexcept
{
    return m_Format;
}

std::uint8_t const *asge::media::Image::Data() const noexcept
{
    return m_Data.data();
}

std::size_t asge::media::Image::Stride() const noexcept
{
    return m_Width * PixelFormatInfoFor(m_Format).s_BytesPerPixel;
}

asge::math::Rect asge::media::Image::AlphaContentBounds() const noexcept
{
    return AlphaContentBounds( math::Rect{ 0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height) } );
}

asge::math::Rect asge::media::Image::AlphaContentBounds( math::Rect const &inRegion ) const noexcept
{
    math::Rect const region = ClampRegionToImage( inRegion, Dimensions() );
    if ( region.w <= 0.0f || region.h <= 0.0f ) return region;

    auto const x0 = static_cast<std::size_t>(region.x);
    auto const y0 = static_cast<std::size_t>(region.y);
    auto const x1 = static_cast<std::size_t>(region.x + region.w); // exclusive
    auto const y1 = static_cast<std::size_t>(region.y + region.h); // exclusive

    std::size_t minX = x1, minY = y1, maxX = x0, maxY = y0;
    bool foundContent = false;

    for ( std::size_t y = y0; y < y1; ++y )
    {
        for ( std::size_t x = x0; x < x1; ++x )
        {
            if ( IsPixelTransparent( *this, x, y ) ) continue;

            foundContent = true;
            minX = std::min( minX, x );
            minY = std::min( minY, y );
            maxX = std::max( maxX, x + 1 ); // exclusive
            maxY = std::max( maxY, y + 1 ); // exclusive
        }
    }

    if ( !foundContent ) return region; // entirely transparent -- fall back to the region itself

    return {
        static_cast<float>(minX), static_cast<float>(minY),
        static_cast<float>(maxX - minX), static_cast<float>(maxY - minY)
    };
}

asge::Result<asge::media::Image> asge::media::Image::Image::Load(filesystem::Path const &inImagePath)
{
    auto byteResult = filesystem::ReadBinary( inImagePath );
    if (!byteResult) return Result<Image>::Err(byteResult.Error());
    return DecodeImage(byteResult.Value());
}

asge::Result<asge::media::Image> asge::media::DecodeImage(std::span<const std::byte> inBytes) noexcept
{
    constexpr int kDesiredChannels = 4; // force RGBA8
    int width = 0;
    int height = 0;
    int channelsInFile = 0;

    stbi_uc* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(inBytes.data()), static_cast<int>(inBytes.size()),
        &width, &height, &channelsInFile, kDesiredChannels
    );

    if ( pixels == nullptr )
    {
        const char* reason = stbi_failure_reason();
        auto const ec = make_error_code( errors::ImageError::DecodeFailed );
        return Result<Image>::Err( ec, reason != nullptr ? reason : "uknonw stb_image error" );
    }

    const auto dataSize = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height)
        * static_cast<std::size_t>(kDesiredChannels);

    std::vector<std::uint8_t> data(pixels, pixels + dataSize);
    stbi_image_free(pixels);

    return Result<Image>::Ok(Image( width, height, PixelFormat::RGBA8, std::move(data)));
}