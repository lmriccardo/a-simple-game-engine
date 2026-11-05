#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <span>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Graphics/PixelFormat.hpp>

namespace asge::media
{

/**
 * @brief A decoded raster image held in CPU memory.
 *
 * Stores the pixel bytes row after row with no padding, together with the
 * width, height and PixelFormat. Move-only. Decode one with Load() or
 * DecodeImage(), which always produce RGBA8.
 */
class Image
{
public:
    using data_t = std::vector<std::uint8_t>;

private:
    std::size_t m_Width;  // The width of the image in pixels
    std::size_t m_Height; // The heigth of the image in pixels
    graphics::PixelFormat m_Format; // The format of the pixels
    data_t      m_Data;   // Bytes representing the image

public:
    Image() = delete;

    /**
     * @brief Builds an image from already-decoded pixel bytes.
     *
     * inData is copied and must hold width * height * bytes-per-pixel bytes;
     * the size is not checked.
     */
    Image( std::size_t inW, std::size_t inH, graphics::PixelFormat inFormat, data_t const& inData );

    Image(Image const&) = delete;
    Image(Image&&) = default;
    Image& operator=(Image const&) = delete;
    Image& operator=(Image&&) = default;

    /**
     * @brief The image size in pixels, as (width, height).
     */
    [[nodiscard]] math::Int2 Dimensions() const noexcept;

    /**
     * @brief The pixel format of the stored bytes.
     */
    [[nodiscard]] graphics::PixelFormat Format() const noexcept;

    /**
     * @brief Pointer to the first pixel byte; valid for the image's lifetime.
     */
    [[nodiscard]] std::uint8_t const* Data() const noexcept;

    /**
     * @brief Number of bytes in one row of pixels (width * bytes-per-pixel).
     */
    [[nodiscard]] std::size_t Stride() const noexcept;

    /**
     * @brief Tight bounding box of this image's non-transparent pixels.
     *
     * Given in the image's own pixel coordinates. Scans the whole image; see
     * the region overload below to scan just part of it. A pixel counts as
     * transparent only when its alpha is exactly 0.
     */
    [[nodiscard]] math::Rect AlphaContentBounds() const noexcept;

    /**
     * @brief Tight bounding box of the non-transparent pixels within inRegion.
     *
     * inRegion is clamped to this image's own bounds and the result is in the
     * image's own pixel coordinates. Falls back to the clamped region itself
     * if every pixel in it is fully transparent.
     */
    [[nodiscard]] math::Rect AlphaContentBounds( math::Rect const& inRegion ) const noexcept;

    /**
     * @brief Reads an image file from disk and decodes it into an RGBA8 Image.
     *
     * The file may be any format stb_image reads (PNG, JPEG, ...). Returns
     * the file-read error if the file can't be read, or an ImageError if the
     * bytes can't be decoded.
     */
    static Result<Image> Load( filesystem::Path const& inImagePath );

    /**
     * @brief Checks whether inPath's extension is one Load() can decode.
     *
     * A cheap, content-blind pre-filter for a file picker (png, jpg, jpeg,
     * bmp, tga, gif; case-insensitive), not a guarantee the bytes themselves
     * are valid.
     */
    [[nodiscard]] static bool IsSupportedFile( filesystem::Path const& inPath ) noexcept;
};

/**
 * @brief Decodes an encoded image (PNG, JPEG, ...) into an RGBA8 Image.
 *
 * inBytes are the raw, still-encoded file bytes. Whatever the source channel
 * count, the result is always converted to four 8-bit channels. Returns an
 * ImageError::DecodeFailed carrying stb_image's reason if decoding fails.
 */
Result<Image> DecodeImage( std::span<const std::byte> inBytes ) noexcept;

}