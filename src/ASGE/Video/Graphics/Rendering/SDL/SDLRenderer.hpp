#pragma once

#include "../../Renderer.hpp"
#include <string>
#include <SDL3/SDL.h>

namespace asge::video
{

/**
 * @brief An owning wrapper around SDL_Renderer.
 */
class SDLRenderer final : public IRenderer
{
private:
    SDL_Renderer * m_Renderer{nullptr}; // The owned native renderer
    Camera         m_Camera{};          // Camera applied to every draw call
    Viewport       m_Viewport{};        // Viewport used for culling and SDL clipping

public:
    /**
     * @brief Creates the SDL renderer for inWindow.
     *
     * Enables alpha blending so the alpha of draw colors takes effect, and
     * defaults the viewport to the window's size. A creation failure is
     * logged and leaves IsValid() false.
     */
    SDLRenderer(SDL_Window* inWindow);

    /**
     * @brief Takes ownership of inOther's SDL renderer.
     *
     * inOther is left empty (invalid). The camera and viewport are not copied.
     */
    SDLRenderer(SDLRenderer&& inOther);

    /**
     * @brief Move-assigns, destroying the current renderer first.
     *
     * Takes inOther's native renderer and leaves inOther empty. Self-assignment is a no-op.
     */
    SDLRenderer& operator=(SDLRenderer&& inOther);

    /**
     * @brief Destroys the SDL renderer.
     *
     * Equivalent to calling Destroy().
     */
    virtual ~SDLRenderer();

    // Renderer is not copyiable at all
    SDLRenderer(SDLRenderer const& inOther) = delete;
    SDLRenderer& operator=(SDLRenderer const& inOther) = delete;

    /**
     * @brief SDL implementation of IRenderer::Clear.
     *
     * Sets the draw color then calls SDL_RenderClear; SDL errors are logged.
     */
    void Clear(graphics::RGBA_Color const& inColor) const override;

    /**
     * @brief SDL implementation of IRenderer::DrawRect.
     *
     * The rect goes through TransformRect with the current camera, then
     * SDL_RenderFillRect or SDL_RenderRect. SDL errors are logged.
     */
    void DrawRect(math::Rect const& inRect, graphics::RGBA_Color const& inColor, bool inFill) const override;

    /**
     * @brief SDL implementation of IRenderer::DrawLine.
     *
     * Both endpoints go through WorldToScreen, then SDL_RenderLine. SDL errors are logged.
     */
    void DrawLine(math::Float2 const& inC1, math::Float2 const& inC2, graphics::RGBA_Color const& inColor) const override;

    /**
     * @brief SDL implementation of IRenderer::DrawCircle.
     *
     * Rasterizes the screen-space circle with MidpointCirclePoints. The outline is
     * drawn as points; the fill draws one horizontal line per row between the
     * leftmost and rightmost outline point. SDL errors are logged.
     */
    void DrawCircle(math::Int2 const& inCenter, int inRadius, graphics::RGBA_Color const& inColor, bool inFill) const override;

    /**
     * @brief SDL implementation of IRenderer::DrawTexture (stretch into a rect).
     *
     * The destination goes through TransformRect, then SDL_RenderTexture. SDL errors are logged.
     */
    void DrawTexture( ITexture const& inTexture, math::Rect const& inDestRect ) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTexture (native size at a position).
     *
     * Builds a rect from inPosition and the texture size, then forwards to the rect overload.
     */
    void DrawTexture(ITexture const& inTexture, math::Float2 const& inPosition) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTexture (source sub-rect to dest rect).
     *
     * inSrcRect is used as is, in texture pixels. Only the destination goes through the camera.
     */
    void DrawTexture(ITexture const& inTexture, math::Rect const& inSrcRect,math::Rect const& inDestRect) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTextureTiled.
     *
     * The tile scale is inScale times the camera zoom, then SDL_RenderTextureTiled is used.
     */
    void DrawTextureTiled(ITexture const& inTexture, float inScale, math::Rect const& inDestRect) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTextureAffine (whole texture).
     *
     * The three corner points go through WorldToScreen, then SDL_RenderTextureAffine.
     */
    void DrawTextureAffine(ITexture const& inTexture, math::Float2 const& inOrigin,
        math::Float2 const& inRight, math::Float2 const& inDown) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTextureAffine (source sub-rect).
     *
     * Same as the whole-texture overload but maps only inSrcRect of the texture.
     */
    void DrawTextureAffine(ITexture const& inTexture, math::Rect const& inSrcRect, math::Float2 const& inOrigin,
        math::Float2 const& inRight, math::Float2 const& inDown) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawTexture9Grid.
     *
     * The destination goes through TransformRect and the camera zoom is passed
     * to SDL_RenderTexture9Grid as the border scale. SDL errors are logged.
     */
    void DrawTexture9Grid(
        ITexture const& inTexture, float inLeft, float inRight, float inTop,
        float inBottom, math::Rect const& inDestRect
    ) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::DrawString.
     *
     * Sets inTexture's color mod to inColor, then walks the UTF-8 text drawing one
     * atlas slice per glyph and advancing the pen. '
' starts a new line; codepoints
     * the font has no glyph for are skipped.
     */
    void DrawString( str::StringView inText, media::Font const& inFont, ITexture & inTexture,
        math::Float2 const& inPosition, graphics::RGBA_Color const& inColor
    ) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::CreateTexture.
     *
     * Always returns an SDLTexture; on upload failure it is invalid (IsValid() false).
     */
    [[nodiscard]] std::unique_ptr<ITexture> CreateTexture( media::Image const& inImage ) const noexcept override;

    /**
     * @brief SDL implementation of IRenderer::Present (SDL_RenderPresent).
     *
     * An SDL error is logged.
     */
    void Present() const override;

    /**
     * @brief SDL implementation of IRenderer::IsValid.
     *
     * True while the native SDL_Renderer exists.
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @brief Returns the owned SDL_Renderer* as a void*.
     *
     * Null after Destroy() or a move.
     */
    [[nodiscard]] void* NativeHandle() const noexcept override;

    /**
     * @brief Stores inCamera; later draw calls transform through it.
     *
     * The camera is only kept here, SDL is not told about it.
     */
    void SetCamera( Camera const& inCamera ) override;

    /**
     * @brief Returns the stored camera.
     *
     * Reflects the last SetCamera call (a default camera before that).
     */
    Camera const& GetCamera() const override;

    /**
     * @brief Stores inViewport and applies it with SDL_SetRenderViewport.
     *
     * The values are truncated to integers for SDL; GetViewport() keeps the float rect.
     */
    void SetViewport(Viewport const& inViewport) override;

    /**
     * @brief Returns the stored viewport.
     *
     * Defaults to the window's full size until SetViewport is called.
     */
    Viewport const& GetViewport() const override;

    /**
     * @brief Destroys the native SDL renderer.
     *
     * Safe to call more than once; afterwards IsValid() is false.
     */
    void Destroy();
};

} // namespace asge::video
