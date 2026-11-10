#include "Angle.hpp"

#include <cmath>

namespace
{

float WrapAngle( float inAngle ) noexcept
{
    float const wrapped = std::fmod( inAngle, asge::math::kTwoPi );
    return wrapped < 0.0F ? wrapped + asge::math::kTwoPi : wrapped;
}

}

float asge::math::AngleOf( float inX, float inY ) noexcept
{
    if ( inX == 0.0F && inY == 0.0F ) return 0.0F;
    return WrapAngle( std::atan2( inY, inX ) );
}

float asge::math::AngularDistance( float inA, float inB ) noexcept
{
    float const d = std::fabs( WrapAngle( inA ) - WrapAngle( inB ) );
    return d > kPi ? kTwoPi - d : d;
}

int asge::math::QuantizeAngle( float inAngle, int inSectors, int inCurrent, float inBias ) noexcept
{
    if ( inSectors <= 0 ) return 0;

    float const step  = kTwoPi / static_cast<float>( inSectors );
    float const angle = WrapAngle( inAngle );
    int const sector  = static_cast<int>( std::lround( angle / step ) ) % inSectors;

    if ( inCurrent < 0 || inCurrent >= inSectors || inCurrent == sector ) return sector;

    float const distCurrent   = AngularDistance( angle, static_cast<float>( inCurrent ) * step );
    float const distCandidate = AngularDistance( angle, static_cast<float>( sector ) * step );
    return distCurrent > inBias * distCandidate ? sector : inCurrent;
}
