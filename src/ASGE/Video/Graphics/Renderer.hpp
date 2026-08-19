#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Graphics/Font.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Core/Errors.hpp>
#include "Texture.hpp"

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

    /**
     * @brief Draw a texture to screen, scaled into the given destination rect
     *
     * @param inTexture the texture to draw
     * @param inDestRect the destination rectangle (position + size) to draw into
     */
    virtual void DrawTexture( ITexture const& inTexture, math::Rect const& inDestRect ) const noexcept = 0;

    /**
     * @brief Draw a texture to screen at native size
     *
     * Draws the texture at the given position using its own width/height
     * (from ITexture::Size()) — no scaling.
     *
     * @param inTexture the texture to draw
     * @param inPosition the top-left position to draw at
     */
    virtual void DrawTexture(ITexture const& inTexture, math::Float2 const& inPosition) const noexcept = 0;

    /**
     * @brief Draw a sub-region of a texture, scaled into the given destination rect
     *
     * Draws only the portion of inTexture within inSrcRect, stretched to fill
     * inDestRect. Used for atlas-backed content (e.g. glyphs) where each draw
     * call needs one small slice of a shared texture rather than the whole thing.
     *
     * @param inTexture the texture to draw from
     * @param inSrcRect the source rectangle within inTexture, in pixels
     * @param inDestRect the destination rectangle (position + size) to draw into
     */
    virtual void DrawTexture(ITexture const& inTexture, math::Rect const& inSrcRect,
        math::Rect const& inDestRect) const noexcept = 0;

    /**
     * @brief Draw a texture using nine-slice (9-grid) scaling
     *
     * Splits the texture into a 3x3 grid using the given margins. Corners are
     * drawn unscaled, edges stretch along one axis, and the center stretches
     * to fill the remaining space — keeps borders/corners crisp regardless of
     * destination size. Typical use: UI panels, buttons, dialog boxes.
     *
     * @param inTexture the texture to draw
     * @param inLeft width of the left margin, in texture pixels
     * @param inRight width of the right margin, in texture pixels
     * @param inTop height of the top margin, in texture pixels
     * @param inBottom height of the bottom margin, in texture pixels
     * @param inDestRect the destination rectangle to fill
     */
    virtual void DrawTexture9Grid(
        ITexture const& inTexture, float inLeft, float inRight, float inTop, float inBottom,
        math::Rect const& inDestRect) const noexcept = 0;

    /**
     * @brief Draw a texture repeated (tiled) to fill a destination rectangle
     *
     * Instead of stretching the source texture to fit, repeats it like a
     * wallpaper pattern, clipped to inDestRect. Typical use: backgrounds,
     * tilemaps, scrolling layers.
     *
     * @param inTexture the texture to draw
     * @param inScale scale applied to each tile before repeating (1.0 = native size)
     * @param inDestRect the destination rectangle to fill
     */
    virtual void DrawTextureTiled(
        ITexture const& inTexture, float inScale, math::Rect const& inDestRect
    ) const noexcept = 0;

    /**
     * @brief Draw a texture using an arbitrary affine transform
     *
     * Maps the texture's (0,0)/(w,0)/(0,h) corners to inOrigin/inRight/inDown
     * on screen. Supports rotation, shear, and non-uniform scale in one call.
     *
     * @param inTexture the texture to draw
     * @param inOrigin where the texture's top-left (0,0) lands on screen
     * @param inRight where the texture's top-right (w,0) lands on screen
     * @param inDown where the texture's bottom-left (0,h) lands on screen
     */
    virtual void DrawTextureAffine(
        ITexture const& inTexture, math::Float2 const& inOrigin, math::Float2 const& inRight, 
        math::Float2 const& inDown) const noexcept = 0;

    /**
     * @brief Draw a string of text using a baked Font and its uploaded atlas texture
     *
     * Renders characters from a fixed baked ASCII range (see Font::Load).
     * Codepoints outside that range are skipped. Tints the shared atlas
     * texture via inTexture's color mod before drawing, so all glyphs in
     * this call share inColor.
     *
     * @param inFont the loaded font providing glyph metrics
     * @param inTexture the atlas texture uploaded from inFont.GetAtlasImage()
     * @param inText the text to draw
     * @param inPosition the baseline-relative pen start position
     * @param inColor tint applied to the (alpha-only) atlas glyphs
     */
    virtual void DrawString( str::StringView inText, graphics::Font const& inFont,
        ITexture & inTexture, math::Float2 const& inPosition,
        graphics::RGBA_Color const& inColor ) const noexcept = 0;

    // Present the redered content to the screen
    virtual void Present() const = 0;

    // Creates a texture starting from the input image
    [[nodiscard]] virtual std::unique_ptr<ITexture> CreateTexture( graphics::Image const& inImage ) const noexcept = 0;

    // Checks if the current renderer is valid or not
    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace asge::video
