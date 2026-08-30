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

bool CircleOverlap( Circle const& inA, Circle const& inB ) noexcept;

/**
 * @brief Minimum translation vector to separate two overlapping AABBs.
 *
 * Returns the smallest push-out along whichever axis (X or Y) has the
 * lesser penetration depth, signed so applying it to inA's position moves
 * it away from inB. std::nullopt if the boxes don't overlap.
 */
std::optional<Float2> PenetrationVector( Rect const& inA, Rect const& inB ) noexcept;

std::optional<Float2> PenetrationVector( Circle const& inA, Circle const& inB ) noexcept;

std::optional<Float2> PenetrationVector( Rect const& inRect, Circle const& inCircle ) noexcept;

std::optional<Float2> PenetrationVector( Circle const& inCircle, Rect const& inRect ) noexcept;

}