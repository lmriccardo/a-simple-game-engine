#pragma once

#include <optional>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

struct Rect
{
    float x; // The x coordinate position
    float y; // The y coordinate position
    float w; // The width of the rectangle
    float h; // The height of the rectangle
};

// intersection methods for rectagles and more over ...

/**
 * @brief Axis-aligned overlap test between two rectangles.
 */
bool AabbOverlap( Rect const& inA, Rect const& inB ) noexcept;

/**
 * @brief Minimum translation vector to separate two overlapping AABBs.
 *
 * Returns the smallest push-out along whichever axis (X or Y) has the
 * lesser penetration depth, signed so applying it to inA's position moves
 * it away from inB. std::nullopt if the boxes don't overlap.
 */
std::optional<Float2> PenetrationVector( Rect const& inA, Rect const& inB ) noexcept;


}