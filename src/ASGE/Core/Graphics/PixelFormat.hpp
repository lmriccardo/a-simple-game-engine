#pragma once

#include <cstddef>

namespace asge::graphics
{

enum class PixelFormat { RGBA8 };

/**
 * @brief Returns the total number of bytes for representing a single pixel.
 */
std::size_t BytesPerPixel( PixelFormat inFormat ) noexcept;

}