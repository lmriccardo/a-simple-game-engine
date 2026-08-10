#pragma once

#include <vector>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

/**
 * @brief Computes a circle's outline points using the midpoint circle algorithm
 *
 * Walks one octant with an integer error term (no trig/floats) and mirrors
 * each point across both axes and the diagonal to cover the full circle.
 *
 * @param inCenter The center of the circle
 * @param inRadius The radius of the circle
 * @return The circle's outline points
 */
std::vector<Int2> MidpointCirclePoints(Int2 inCenter, int inRadius);

}