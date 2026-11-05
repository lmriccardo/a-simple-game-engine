#pragma once

#include <optional>
#include "Circle.hpp"
#include "Rect.hpp"

namespace asge::math
{

/**
 * @brief Axis-aligned overlap test between two rectangles.
 *
 * Strict: rectangles that only share an edge or corner do not overlap.
 */
bool AabbOverlap( Rect const& inA, Rect const& inB ) noexcept;

/**
 * @brief Overlap test between two circles.
 *
 * Compares the squared distance between centers with the squared sum of the
 * radii. Strict, like AabbOverlap: circles that only touch do not overlap.
 */
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
 * Points along the line between the two centers, with a length equal to the
 * penetration depth. Note that the direction runs from inA's center toward
 * inB's center, the opposite sign of the Rect overloads, so subtract it to
 * move inA away. std::nullopt if they don't overlap; if the centers exactly
 * coincide, the result is (summed radii, 0) arbitrarily along +X.
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

/**
 * @brief PenetrationVector(Rect, Circle) with the push direction flipped.
 *
 * Applying it to inCircle's position moves the circle away from the rect
 * instead. std::nullopt if they don't overlap.
 */
std::optional<Float2> PenetrationVector( Circle const& inCircle, Rect const& inRect ) noexcept;

}