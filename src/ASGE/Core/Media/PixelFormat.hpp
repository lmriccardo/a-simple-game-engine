#pragma once

#include <cstddef>

namespace asge::media
{

struct PixelFormatInfo 
{ 
    std::size_t s_BytesPerPixel; // The total number of bytes for representing a single pixel
};

enum class PixelFormat { RGBA8, A8 };

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