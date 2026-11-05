#pragma once

#include <vector>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include "Rect.hpp"

namespace asge::math
{

/**
 * @brief A circle, used interchangeably with Rect as a collidable shape.
 *
 * See Collision.hpp's AabbOverlap/PenetrationVector family. It has no default
 * values, so a default-initialized Circle holds garbage.
 */
struct Circle
{
    math::Float2 m_Center; // center point, in whatever space the caller is working in
    float        m_Radius; // radius, in the same units as m_Center
};

/**
 * @brief Computes a circle's outline points using the midpoint circle algorithm
 *
 * Walks one octant with an integer error term (no trig/floats) and mirrors
 * each point across both axes and the diagonal to cover the full circle.
 * The outline is one pixel thick and a few points are listed twice (where
 * the octants meet). A radius of 0 gives only the center; a negative radius
 * gives no points.
 */
std::vector<Int2> MidpointCirclePoints(Int2 inCenter, int inRadius);

/**
 * @brief The smallest Rect containing the circle.
 *
 * A square of side 2 * radius whose top-left corner is (center - radius).
 */
Rect BoundingRect( Circle const& inCircle ) noexcept;

}