#include "PixelFormat.hpp"

std::size_t asge::graphics::BytesPerPixel( PixelFormat inFormat ) noexcept
{
    switch (inFormat)
    {
    case PixelFormat::RGBA8: return 4;
    }

    return 0;
}
