#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <span>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include "PixelFormat.hpp"

namespace asge::graphics
{

class Image
{
public:
    using data_t = std::vector<std::uint8_t>;

private:
    std::size_t m_Width;  // The width of the image in pixels
    std::size_t m_Height; // The heigth of the image in pixels
    PixelFormat m_Format; // The format of the pixels
    data_t      m_Data;   // Bytes representing the image

public:
    Image() = delete;
    Image( std::size_t inW, std::size_t inH, PixelFormat inFormat, data_t const& inData );

    Image(Image const&) = delete;
    Image(Image&&) = default;
    Image& operator=(Image const&) = delete;
    Image& operator=(Image&&) = default;

    [[nodiscard]] math::Int2 Dimensions() const noexcept;
    [[nodiscard]] PixelFormat Format() const noexcept;
    [[nodiscard]] std::uint8_t const* Data() const noexcept;
    [[nodiscard]] std::size_t Stride() const noexcept;
};

/**
 * @brief Decodes an encoded image (PNG, JPEG, ...) into an RGBA8 Image
 *
 * @param inBytes The raw, still-encoded file bytes
 * @return The decoded Image, or an ImageError if the bytes couldn't be decoded
 */
Result<Image> DecodeImage( std::span<const std::byte> inBytes ) noexcept;

/**
 * @brief Reads an image file from disk and decodes it into an RGBA8 Image
 *
 * Named ReadImage, not LoadImage -- Windows headers #define LoadImage to
 * LoadImageA/W (same reason FileIO.hpp's Copy isn't named CopyFile).
 *
 * @param inImagePath The path to the encoded image file (PNG, JPEG, ...)
 * @return The decoded Image, or an error if the file couldn't be read or decoded
 */
Result<Image> ReadImage( filesystem::Path const& inImagePath );

}