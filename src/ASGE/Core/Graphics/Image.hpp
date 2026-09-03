#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <span>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
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

    /**
     * @brief Tight bounding box of this image's non-transparent pixels, in
     *        its own pixel coordinates. Scans the whole image; see the
     *        region overload below to scan just part of it.
     */
    [[nodiscard]] math::Rect AlphaContentBounds() const noexcept;

    /**
     * @brief Tight bounding box of the non-transparent pixels within
     *        inRegion (clamped to this image's own bounds), in the image's
     *        own pixel coordinates. Falls back to the clamped region itself
     *        if every pixel in it is fully transparent.
     */
    [[nodiscard]] math::Rect AlphaContentBounds( math::Rect const& inRegion ) const noexcept;

    /**
     * @brief Reads an image file from disk and decodes it into an RGBA8 Image
     *
     * @param inImagePath The path to the encoded image file (PNG, JPEG, ...)
     * @return The decoded Image, or an error if the file couldn't be read or decoded
     */
    static Result<Image> Load( filesystem::Path const& inImagePath );
};

/**
 * @brief Decodes an encoded image (PNG, JPEG, ...) into an RGBA8 Image
 *
 * @param inBytes The raw, still-encoded file bytes
 * @return The decoded Image, or an ImageError if the bytes couldn't be decoded
 */
Result<Image> DecodeImage( std::span<const std::byte> inBytes ) noexcept;

/**
 * @brief Builds inCount evenly-spaced sub-rects (in texture pixels) across a
 *        regular grid spritesheet, for use as components::Animation::m_Frames.
 *
 * inSheetCell's x/y is the top-left of frame 0 and its w/h is one cell's
 * size; frame i sits at column (i % inColumns), row (i / inColumns) of that
 * grid, so inCount need not fill the grid exactly (e.g. 6 frames out of an
 * 8-cell sheet). Returns an empty vector if inColumns or inCount is 0.
 */
std::vector<math::Rect> MakeGridFrames(
    math::Rect inSheetCell, std::size_t inColumns, std::size_t inCount ) noexcept;

}