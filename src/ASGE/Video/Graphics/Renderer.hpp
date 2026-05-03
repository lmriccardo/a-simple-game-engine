#pragma once

#include "Color.hpp"
#include <ASGE/Math/Rect.hpp>

namespace asge::graphics
{

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // Clear the screen content
    virtual void Clear(RGBA_Color const& inColor) const = 0;

    /**
     * @brief Draw a rectangle to screen
     * 
     * Draw a rectangle to screen given the input position (X, Y), the 
     * dimension (W = width, H = height) and the filling color (RGBA).
     * 
     * @param inX the x coordinate value position
     * @param inY the y coordinate value position
     * @param inW the input width
     * @param inH the input height
     * @param inColor the input RGBA color
     */
    virtual void DrawRect(math::Rect const& inRect, RGBA_Color const& inColor) const = 0;
    virtual void DrawRect(float x, float y, float w, float h, 
        RGBA_Color const& inColor) const = 0;

    // Present the redered content to the screen
    virtual void Present() const = 0;

    // Checks if the current renderer is valid or not
    virtual bool IsValid() const = 0;
};

} // namespace asge::graphics
