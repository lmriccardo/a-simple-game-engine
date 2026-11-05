#pragma once

#include <numbers>

namespace asge::math
{

inline constexpr float kPi    = std::numbers::pi_v<float>; // Half a turn, in radians
inline constexpr float kTwoPi = 2.0F * kPi;                // A full turn, in radians

/**
 * @brief The counter-clockwise angle of the vector (inX, inY) from the +X axis.
 *
 * The result is in [0, 2π), with +Y taken as up. For screen coordinates,
 * where Y points down, pass -Y to get a counter-clockwise angle. The zero
 * vector has no direction and returns 0.
 */
[[nodiscard]] float AngleOf( float inX, float inY ) noexcept;

/**
 * @brief The shortest angular distance between two angles, in radians.
 *
 * The result is in [0, π] and accounts for wrap-around, so 0.1 and 2π - 0.1
 * are 0.2 apart, not nearly a full turn. The inputs may be any angle, not
 * only ones in [0, 2π).
 */
[[nodiscard]] float AngularDistance( float inA, float inB ) noexcept;

/**
 * @brief Snaps an angle to the nearest of inSectors evenly spaced directions.
 *
 * Sector k is centered on k * 2π / inSectors, counter-clockwise from +X, so
 * sector 0 covers both sides of the +X axis. Returns the sector index in
 * [0, inSectors). If inCurrent is a valid sector, it is kept unless the angle
 * is closer to the new sector by a factor of inBias (hysteresis), so a heading
 * sitting on a sector boundary does not flicker. Pass -1 for no hysteresis.
 */
[[nodiscard]] int QuantizeAngle(
    float inAngle, int inSectors, int inCurrent = -1, float inBias = 1.0F ) noexcept;

}
