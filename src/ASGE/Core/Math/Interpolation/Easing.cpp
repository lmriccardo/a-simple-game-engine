#include "Easing.hpp"

#include <algorithm>

float asge::math::Ease( Easing inEasing, float inT ) noexcept
{
    float const t = std::clamp( inT, 0.0f, 1.0f );

    switch ( inEasing )
    {
    case Easing::InQuad:    return t * t;
    case Easing::OutQuad:   return t * ( 2.0f - t );
    case Easing::InOutQuad: return t < 0.5f ? 2.0f * t * t : -1.0f + ( 4.0f - 2.0f * t ) * t;
    case Easing::OutCubic:  { float const u = 1.0f - t; return 1.0f - u * u * u; }
    case Easing::Linear:    break;
    }

    return t;
}
