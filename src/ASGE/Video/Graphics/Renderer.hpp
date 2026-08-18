#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::video
{

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // Clear the screen content
    virtual void Clear(graphics::RGBA_Color const& inColor) const = 0;

    /**
     * @brief Draw a rectangle to screen
     *
     * Draw a rectangle to screen given the input position (X, Y), the
     * dimension (W = width, H = height) and the filling color (RGBA).
     */
    virtual void DrawRect(math::Rect const& inRect, graphics::RGBA_Color const& inColor, bool inFill) const = 0;

    virtual void DrawLine(math::Float2 const& inC1, math::Float2 const& inC2,
        graphics::RGBA_Color const& inColor) const = 0;

    /**
     * @brief Draw a circle to screen
     *
     * Draws the outline (or filled interior) of a circle, rasterized via
     * math::MidpointCirclePoints.
     */
    virtual void DrawCircle(math::Int2 const& inCenter, int inRadius,
        graphics::RGBA_Color const& inColor, bool inFill) const = 0;

    // Present the redered content to the screen
    virtual void Present() const = 0;

    // Checks if the current renderer is valid or not
    virtual bool IsValid() const = 0;
};

} // namespace asge::video
