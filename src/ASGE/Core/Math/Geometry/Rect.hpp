#pragma once

#include <optional>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

struct Rect
{
    float m_X;      // the x coordinate position
    float m_Y;      // the y coordinate position
    float m_Width;  // the width of the rectangle
    float m_Height; // the height of the rectangle
};

/** @brief True if inP falls strictly inside inRect -- a point exactly on an edge does not count. */
bool Contains( Rect const& inRect, Float2 const& inP ) noexcept;

/** @brief True if inRect2 lies fully inside inRect1 -- unlike the point overload, shared edges count as inside. */
bool Contains( Rect const& inRect1, Rect const& inRect2 ) noexcept;

/** @brief Computes the distance between a Rect and a point */
float Distance( Rect const& inRect, Float2 const& inP ) noexcept;

}
