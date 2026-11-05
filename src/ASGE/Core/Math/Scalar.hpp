#pragma once

namespace asge::math
{

/**
 * @brief Maps a value from [inMin, inMax] to [0, 1] without clamping.
 *
 * Values outside the range land outside [0, 1]. Returns 0 for an empty range
 * (inMin == inMax) instead of dividing by zero.
 */
inline constexpr float Normalize01(float inValue, float inMin, float inMax) noexcept
{
    if ( inMin == inMax ) return 0.0f;
    return ( inValue - inMin ) / ( inMax - inMin );
}

/**
 * @brief Restricts a value to the closed range [inMin, inMax].
 *
 * Requires inMin <= inMax; otherwise the result is unspecified.
 */
inline constexpr float Clamp( float inValue, float inMin, float inMax ) noexcept
{
    if ( inValue < inMin ) return inMin;
    if ( inValue > inMax ) return inMax;
    return inValue;
}

/**
 * @brief Linearly interpolates from inA to inB by inT.
 *
 * inT is not clamped, so values outside [0, 1] extrapolate.
 */
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

/**
 * @brief Restricts a value to [0, 1].
 *
 * Shorthand for Clamp(inValue, 0, 1).
 */
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