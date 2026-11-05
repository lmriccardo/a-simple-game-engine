#pragma once

#include <cstddef>

namespace asge::graphics
{

struct PixelFormatInfo
{
    std::size_t s_BytesPerPixel; // The total number of bytes for representing a single pixel
};

/**
 * @brief How the bytes of one pixel are laid out.
 *
 * RGBA8 is four 8-bit channels (R, G, B, A in memory order); A8 is a single
 * 8-bit alpha/coverage channel, as used by glyph atlases.
 */
enum class PixelFormat { RGBA8, A8 };

/**
 * @brief Returns the size information of a pixel format.
 *
 * Returns a zero-initialized PixelFormatInfo (0 bytes) for a value outside
 * the enumeration.
 */
inline constexpr PixelFormatInfo PixelFormatInfoFor( PixelFormat f ) noexcept
{
    switch ( f )
    {
    case PixelFormat::RGBA8: return { 4 };
    case PixelFormat::A8: return { 1 };
    }
    return {};
}

}