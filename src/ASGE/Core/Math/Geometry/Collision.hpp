#pragma once

#include <optional>
#include "Circle.hpp"
#include "Rect.hpp"

namespace asge::math
{

/**
 * @brief Axis-aligned overlap test between two rectangles.
 */
bool AabbOverlap( Rect const& inA, Rect const& inB ) noexcept;

/** @brief Overlap test between two circles (distance between centers vs. summed radii). */
bool CircleOverlap( Circle const& inA, Circle const& inB ) noexcept;

/**
 * @brief Minimum translation vector to separate two overlapping AABBs.
 *
 * Returns the smallest push-out along whichever axis (X or Y) has the
 * lesser penetration depth, signed so applying it to inA's position moves
 * it away from inB. std::nullopt if the boxes don't overlap.
 */
std::optional<Float2> PenetrationVector( Rect const& inA, Rect const& inB ) noexcept;

/**
 * @brief Minimum translation vector to separate two overlapping circles.
 *
 * Pushes along the line between the two centers, signed so applying it to
 * inA's position moves it away from inB. std::nullopt if they don't
 * overlap; if the centers exactly coincide, pushes along +X arbitrarily.
 */
std::optional<Float2> PenetrationVector( Circle const& inA, Circle const& inB ) noexcept;

/**
 * @brief Minimum translation vector to separate an overlapping Rect and Circle.
 *
 * Signed so applying it to inRect's position moves the rect away from the
 * circle. std::nullopt if they don't overlap. See PenetrationVector(Circle,
 * Rect) for the same pair with the push direction flipped.
 */
std::optional<Float2> PenetrationVector( Rect const& inRect, Circle const& inCircle ) noexcept;

/** @brief PenetrationVector(Rect, Circle) with the push direction flipped — moves the circle away from the rect instead. */
std::optional<Float2> PenetrationVector( Circle const& inCircle, Rect const& inRect ) noexcept;

}