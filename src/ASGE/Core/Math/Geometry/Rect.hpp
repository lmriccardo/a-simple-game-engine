#pragma once

#include <optional>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

/**
 * @brief An axis-aligned rectangle given by its top-left corner and size.
 *
 * Y grows downwards (screen space), so (m_X, m_Y) is the top-left corner and
 * the rectangle spans to (m_X + m_Width, m_Y + m_Height). It has no default
 * values, so a default-initialized Rect holds garbage.
 */
struct Rect
{
    float m_X;      // the x coordinate position
    float m_Y;      // the y coordinate position
    float m_Width;  // the width of the rectangle
    float m_Height; // the height of the rectangle
};

/**
 * @brief True if inP falls strictly inside inRect.
 *
 * A point exactly on an edge does not count, so a rectangle with zero width
 * or height contains nothing.
 */
bool Contains( Rect const& inRect, Float2 const& inP ) noexcept;

/**
 * @brief True if inRect2 lies fully inside inRect1.
 *
 * Unlike the point overload, shared edges count as inside, so a rectangle
 * contains itself.
 */
bool Contains( Rect const& inRect1, Rect const& inRect2 ) noexcept;

/**
 * @brief Computes the distance between a Rect and a point.
 *
 * Measured from the point to the closest point of the rectangle, so it is 0
 * for a point inside or on the edge.
 */
float Distance( Rect const& inRect, Float2 const& inP ) noexcept;

/**
 * @brief Merges two rects into a single bigger rect.
 *
 * Returns the smallest rectangle containing both, even if they are apart
 * (the gap between them is included).
 */
Rect Merge( Rect const& inR1, Rect const& inR2 ) noexcept;

}
