#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

asge::graphics::Image::Image(
    std::size_t inW, std::size_t inH, PixelFormat inFormat, data_t const &inData
) : m_Width(inW), m_Height(inH), m_Format(inFormat), m_Data(inData)
{}

asge::math::Int2 asge::graphics::Image::Dimensions() const noexcept
{
    return { static_cast<int>(m_Width), static_cast<int>(m_Height) };
}

asge::graphics::PixelFormat asge::graphics::Image::Format() const noexcept
{
    return m_Format;
}

std::uint8_t const *asge::graphics::Image::Data() const noexcept
{
    return m_Data.data();
}

std::size_t asge::graphics::Image::Stride() const noexcept
{
    return m_Width * BytesPerPixel(m_Format);
}

asge::Result<asge::graphics::Image> asge::graphics::DecodeImage(std::span<const std::byte> inBytes) noexcept
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

asge::Result<asge::graphics::Image> asge::graphics::ReadImage(filesystem::Path const &inImagePath)
{
    auto byteResult = filesystem::ReadBinary( inImagePath );
    if (!byteResult) return Result<Image>::Err(byteResult.Error());
    return DecodeImage(byteResult.Value());
}
