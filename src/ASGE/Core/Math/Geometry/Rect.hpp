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

}
