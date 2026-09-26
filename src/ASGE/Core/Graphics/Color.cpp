#include "Color.hpp"

asge::graphics::Color32 asge::graphics::RGBATo32(
    RGBA_Color inColor) noexcept
{
    Color32 result = ( inColor.r << 16 )
                 |   ( inColor.g << 8  )
                 |   ( inColor.b );
    return result;
}

asge::graphics::Color32A asge::graphics::RGBATo32A(RGBA_Color inColor) noexcept
{
    // inColor.a is a uint8_t -- promoted to (32-bit) int by <<, so shifting
    // it by 32 directly is undefined behaviour (shift >= the type's width).
    // Widen to Color32A first so the shift actually lands in the high bits.
    Color32A result = ( static_cast<Color32A>(inColor.a) << 32 ) | RGBATo32( inColor );
    return result;
}

asge::graphics::RGBA_Color asge::graphics::C32ToRGBA(Color32 inColor32) noexcept
{
    RGBA_Color result;
    result.r = ( inColor32 & 0xFF0000 ) >> 16;
    result.g = ( inColor32 & 0x00FF00 ) >> 8;
    result.b = ( inColor32 & 0x0000FF );
    return result;
}

asge::graphics::RGBA_Color asge::graphics::C32AToRGBA(Color32A inColor32A) noexcept
{
    auto result = C32ToRGBA( static_cast<Color32>(inColor32A) );
    result.a = static_cast<std::uint8_t>( ( inColor32A & 0xFF00000000ULL ) >> 32 );
    return result;
}
