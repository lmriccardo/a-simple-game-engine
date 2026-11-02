#pragma once

namespace asge::math
{

/** @brief Maps @p inValue from [@p inMin, @p inMax] to [0, 1] without clamping; returns 0 for an empty range. */
inline constexpr float Normalize01(float inValue, float inMin, float inMax) noexcept
{
    if ( inMin == inMax ) return 0.0f;
    return ( inValue - inMin ) / ( inMax - inMin );
}

/** @brief Restricts @p inValue to the closed range [@p inMin, @p inMax]; requires inMin <= inMax. */
inline constexpr float Clamp( float inValue, float inMin, float inMax ) noexcept
{
    if ( inValue < inMin ) return inMin;
    if ( inValue > inMax ) return inMax;
    return inValue;
}

/** @brief Linearly interpolates from @p inA to @p inB by @p inT, without clamping @p inT. */
inline constexpr float Lerp( float inA, float inB, float inT ) noexcept
{
    return inA + ( inB - inA ) * inT;
}

/**
 * @brief Maps @p inValue from [@p inInMin, @p inInMax] to [@p inOutMin, @p inOutMax] without clamping.
 *
 * An empty input range yields @p inOutMin; values outside the input range extrapolate.
 */
inline constexpr float Remap(
    float inValue, float inInMin, float inInMax, float inOutMin, float inOutMax ) noexcept
{
    return Lerp( inOutMin, inOutMax, Normalize01( inValue, inInMin, inInMax ) );
}

/** @brief Restricts @p inValue to [0, 1]. */
inline constexpr float Saturate( float inValue ) noexcept
{
    return Clamp( inValue, 0.0f, 1.0f );
}

/**
 * @brief Returns true if @p inA and @p inB differ by at most @p inEpsilon.
 *
 * Uses an absolute tolerance, so it suits values near 1; it is not meant for very large or tiny magnitudes.
 */
inline constexpr bool Approx( float inA, float inB, float inEpsilon = 1e-5f ) noexcept
{
    float const diff = inA - inB;
    return ( diff < 0.0f ? -diff : diff ) <= inEpsilon;
}

}