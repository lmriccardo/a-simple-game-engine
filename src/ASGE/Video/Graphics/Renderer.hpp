#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Core/Errors.hpp>
#include "Texture.hpp"
#include "Camera.hpp"

namespace asge::video
{

class IRenderer
{
public:
    /**
     * @brief Virtual destructor so derived types are destroyed through the interface.
     */
    virtual ~IRenderer() = default;

    /**
     * @brief Clears the whole render target to inColor.
     *
     * Call once at the start of a frame, before any draw call. It ignores
     * the camera and viewport transform and fills the entire target.
     */
    virtual void Clear(graphics::RGBA_Color const& inColor) const = 0;

    /**
     * @brief Draw a rectangle to screen
     *
     * Draws inRect (position X/Y plus width/height) in world space, mapped
     * through the current camera. inFill selects a solid fill or a one pixel
     * outline, both in inColor (RGBA).
     */
    virtual void DrawRect(math::Rect const& inRect, graphics::RGBA_Color const& inColor, bool inFill) const = 0;

    /**
     * @brief Draw a line segment to screen
     *
     * Connects inC1 to inC2 in world space, mapped through the current
     * camera, one pixel wide and in inColor.
     */
    virtual void DrawLine(math::Float2 const& inC1, math::Float2 const& inC2,
        graphics::RGBA_Color const& inColor) const = 0;

    /**
     * @brief Draw a circle to screen
     *
     * Draws the outline (or filled interior) of a circle, rasterized via
     * math::MidpointCirclePoints, centered on inCenter with radius inRadius
     * in pixels. The camera maps the center and scales the radius by its zoom.
     */
    virtual void DrawCircle(math::Int2 const& inCenter, int inRadius,
        graphics::RGBA_Color const& inColor, bool inFill) const = 0;

    /**
     * @brief Draw a texture to screen, scaled into the given destination rect
     *
     * The whole of inTexture is stretched to fill inDestRect, so a destination
     * of a different size or aspect ratio distorts it. inDestRect is in world
     * space and goes through the current camera.
     */
    virtual void DrawTexture( ITexture const& inTexture, math::Rect const& inDestRect ) const noexcept = 0;

    /**
     * @brief Draw a texture to screen at native size
     *
     * Draws the texture with its top-left corner at inPosition using its own
     * width/height (from ITexture::Size()), with no scaling.
     */
    virtual void DrawTexture(ITexture const& inTexture, math::Float2 const& inPosition) const noexcept = 0;

    /**
     * @brief Draw a sub-region of a texture, scaled into the given destination rect
     *
     * Draws only the portion of inTexture within inSrcRect (in texture
     * pixels), stretched to fill inDestRect. Used for atlas-backed content
     * (e.g. glyphs) where each draw call needs one small slice of a shared
     * texture rather than the whole thing.
     */
    virtual void DrawTexture(ITexture const& inTexture, math::Rect const& inSrcRect,
        math::Rect const& inDestRect) const noexcept = 0;

    /**
     * @brief Draw a texture using nine-slice (9-grid) scaling
     *
     * Splits the texture into a 3x3 grid using the margins inLeft, inRight,
     * inTop and inBottom (in texture pixels). Corners are drawn unscaled,
     * edges stretch along one axis, and the center stretches to fill the
     * remaining space of inDestRect, keeping borders crisp at any size.
     * Typical use: UI panels, buttons, dialog boxes.
     */
    virtual void DrawTexture9Grid(
        ITexture const& inTexture, float inLeft, float inRight, float inTop, float inBottom,
        math::Rect const& inDestRect) const noexcept = 0;

    /**
     * @brief Draw a texture repeated (tiled) to fill a destination rectangle
     *
     * Instead of stretching the source texture to fit, repeats it like a
     * wallpaper pattern, clipped to inDestRect. inScale scales each tile
     * before repeating (1.0 is native size). Typical use: backgrounds,
     * tilemaps, scrolling layers.
     */
    virtual void DrawTextureTiled(
        ITexture const& inTexture, float inScale, math::Rect const& inDestRect
    ) const noexcept = 0;

    /**
     * @brief Draw a texture using an arbitrary affine transform
     *
     * Maps the texture's (0,0)/(w,0)/(0,h) corners to inOrigin/inRight/inDown
     * on screen. Supports rotation, shear, and non-uniform scale in one call.
     * The fourth corner is implied by the parallelogram these three define.
     */
    virtual void DrawTextureAffine(
        ITexture const& inTexture, math::Float2 const& inOrigin, math::Float2 const& inRight,
        math::Float2 const& inDown) const noexcept = 0;

    /**
     * @brief Draw a sub-region of a texture using an arbitrary affine transform
     *
     * Same mapping as the whole-texture DrawTextureAffine overload, except
     * inOrigin/inRight/inDown correspond to inSrcRect's own top-left/
     * top-right/bottom-left corners rather than the whole texture's --
     * needed to draw a rotated sprite cropped to one cell of a larger
     * spritesheet (e.g. an animation frame).
     */
    virtual void DrawTextureAffine(
        ITexture const& inTexture, math::Rect const& inSrcRect, math::Float2 const& inOrigin,
        math::Float2 const& inRight, math::Float2 const& inDown) const noexcept = 0;

    /**
     * @brief Draws a string of text using a baked Font and its uploaded atlas texture.
     *
     * Decodes inText as UTF-8 and renders the fixed baked ASCII range (see
     * Font::Load); other codepoints are skipped. A newline returns the pen to
     * inPosition's x and moves down one line. inTexture must be the atlas uploaded
     * from inFont.GetAtlasImage(); all glyphs share inColor.
     */
    virtual void DrawString( str::StringView inText, media::Font const& inFont,
        ITexture & inTexture, math::Float2 const& inPosition,
        graphics::RGBA_Color const& inColor ) const noexcept = 0;

    /**
     * @brief Draws a texture multiplied by a tint color.
     *
     * inTint's RGB scales the pixels and its alpha the opacity (white at full
     * alpha draws it unchanged). The texture's own color and blend mode are put
     * back afterwards, so the tint never leaks into later draws of it. The
     * default implementation is built on the other virtuals; a backend may override it.
     */
    virtual void DrawTextureTinted( ITexture& inTexture, math::Rect const& inDestRect,
        graphics::RGBA_Color inTint ) const noexcept;

    /**
     * @brief Draws a texture additively, so it brightens what is under it.
     *
     * inIntensity (clamped to 0..1) scales the glow, 0 adds nothing and 1 adds
     * the texture at full strength. inColor tints the glow. The texture's own
     * color and blend mode are put back afterwards. The default implementation
     * is built on the other virtuals; a backend may override it.
     */
    virtual void DrawTextureGlow( ITexture& inTexture, math::Rect const& inDestRect, float inIntensity,
        graphics::RGBA_Color inColor = graphics::colors::s_White ) const noexcept;

    /**
     * @brief Covers the whole viewport with inColor at inAmount opacity.
     *
     * inAmount (clamped to 0..1) multiplies inColor's own alpha: ramp it 0 to 1
     * for a fade-out and 1 to 0 for a fade-in. It follows the viewport whatever
     * the camera is, so call it after the world is drawn. The default
     * implementation is built on DrawRect; a backend may override it.
     */
    virtual void DrawFade( graphics::RGBA_Color inColor, float inAmount ) const noexcept;

    /**
     * @brief Presents the rendered frame to the screen.
     *
     * Call once at the end of a frame, after every draw call. Nothing drawn
     * since the last Present() is visible until it runs.
     */
    virtual void Present() const = 0;

    /**
     * @brief Creates a GPU texture starting from the input image.
     *
     * Uploads the pixels of inImage to the backend. The returned texture is
     * independent of inImage afterwards. On upload failure the error is logged
     * and the texture comes back invalid, so check ITexture::IsValid().
     */
    [[nodiscard]] virtual std::unique_ptr<ITexture> CreateTexture( media::Image const& inImage ) const noexcept = 0;

    /**
     * @brief Creates an empty texture that can be drawn into.
     *
     * The texture starts transparent black and has size inSize in pixels.
     * Pass it to SetRenderTarget to render into it, then draw it like any
     * other texture. On failure the error is logged and the texture comes
     * back invalid, so check ITexture::IsValid().
     */
    [[nodiscard]] virtual std::unique_ptr<ITexture> CreateRenderTarget( math::Int2 inSize ) const noexcept = 0;

    /**
     * @brief Redirects every draw call into inTarget, or back to the window.
     *
     * inTarget must come from CreateRenderTarget on this renderer and outlive
     * its use; nullptr restores the window. Clear and the camera apply to the
     * active target. Failures are logged and leave the target unchanged.
     */
    virtual void SetRenderTarget( ITexture* inTarget ) const noexcept = 0;

    /**
     * @brief Checks if the current renderer is valid or not.
     *
     * False once the renderer was destroyed or failed to initialise.
     */
    [[nodiscard]] virtual bool IsValid() const = 0;

    /**
     * @brief Returns the backend-native renderer handle (e.g. SDL_Renderer*).
     *
     * Escape hatch for backend-specific code; cast it to the backend type.
     * The renderer still owns the handle.
     */
    [[nodiscard]] virtual void* NativeHandle() const noexcept = 0;

    /**
     * @brief Sets the camera every subsequent draw call is transformed through
     *
     * Every draw call (DrawRect, DrawTexture*, DrawLine, DrawCircle, ...)
     * maps its world-space coordinates to screen space via inCamera before
     * rendering -- see Camera.hpp's WorldToScreen/TransformRect.
     */
    virtual void SetCamera( Camera const& inCamera ) = 0;

    /**
     * @brief Returns the camera currently applied to draw calls.
     *
     * See SetCamera. The reference is valid until the next SetCamera call.
     */
    virtual Camera const& GetCamera() const = 0;

    /**
     * @brief Sets the screen-space viewport draw calls are clipped/offset into
     *
     * Backed by the native renderer's own viewport support (e.g.
     * SDL_SetRenderViewport), so clipping outside inViewport is handled by
     * the backend rather than by this class.
     */
    virtual void SetViewport(Viewport const& inViewport) = 0;

    /**
     * @brief Returns the viewport currently applied to draw calls.
     *
     * See SetViewport. The reference is valid until the next SetViewport call.
     */
    virtual Viewport const& GetViewport() const = 0;
};

/**
 * @brief RAII guard that renders into a target and restores the window on exit.
 *
 * Construction calls IRenderer::SetRenderTarget with the target and
 * destruction calls it with nullptr. Not copyable or movable; the renderer
 * and the target must outlive it.
 */
class RenderTargetScope
{
private:
    IRenderer const& m_Renderer; // Renderer whose target is switched

public:
    /**
     * @brief Redirects inRenderer's draw calls into inTarget.
     */
    RenderTargetScope( IRenderer const& inRenderer, ITexture& inTarget ) noexcept
        : m_Renderer( inRenderer )
    {
        m_Renderer.SetRenderTarget( &inTarget );
    }

    /**
     * @brief Sends draw calls back to the window.
     */
    ~RenderTargetScope() { m_Renderer.SetRenderTarget( nullptr ); }

    /**
     * @brief Copy construction is disabled.
     */
    RenderTargetScope( RenderTargetScope const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    RenderTargetScope& operator=( RenderTargetScope const& ) = delete;
};

/**
 * @brief Draws a source rect anchored at a normalized point within it.
 *
 * inSrcAnchor (normalized 0..1 within inSrcRect) lands at inDestRect's
 * (x, y) instead of its top-left corner; {0,0} reproduces plain
 * DrawTexture(srcRect, destRect). Lets one fixed anchor stay put on screen
 * while inSrcRect's size varies call to call (e.g. differently-cropped
 * animation frames).
 */
inline void DrawTextureAnchored(
    IRenderer const& inRenderer, ITexture const& inTexture,
    math::Rect const& inSrcRect, math::Rect const& inDestRect,
    math::Float2 const& inSrcAnchor = math::Float2{ 0.0f, 0.0f }
) noexcept
{
    math::Rect const adjustedDest{
        inDestRect.m_X - inSrcAnchor.x() * inDestRect.m_Width,
        inDestRect.m_Y - inSrcAnchor.y() * inDestRect.m_Height,
        inDestRect.m_Width, inDestRect.m_Height
    };
    inRenderer.DrawTexture( inTexture, inSrcRect, adjustedDest );
}

} // namespace asge::video
