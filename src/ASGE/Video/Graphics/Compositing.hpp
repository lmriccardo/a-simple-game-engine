#pragma once

#include "Renderer.hpp"

namespace asge::video
{

/**
 * @brief Draws a texture multiplied by a tint color.
 *
 * inTint's RGB scales the pixels and its alpha the opacity (white at full
 * alpha draws it unchanged). The texture's own color and blend mode are put
 * back afterwards, so the tint never leaks into later draws of it.
 */
void DrawTextureTinted( IRenderer const& inRenderer, ITexture& inTexture,
    math::Rect const& inDestRect, graphics::RGBA_Color inTint ) noexcept;

/**
 * @brief Draws a texture additively, so it brightens what is under it.
 *
 * inIntensity (clamped to 0..1) scales the glow, 0 adds nothing and 1 adds
 * the texture at full strength. inColor tints the glow. The texture's own
 * color and blend mode are put back afterwards.
 */
void DrawTextureGlow( IRenderer const& inRenderer, ITexture& inTexture,
    math::Rect const& inDestRect, float inIntensity,
    graphics::RGBA_Color inColor = graphics::colors::s_White ) noexcept;

/**
 * @brief Covers the whole viewport with inColor at inAmount opacity.
 *
 * inAmount (clamped to 0..1) multiplies inColor's own alpha: ramp it 0 to 1
 * for a fade-out and 1 to 0 for a fade-in. Drawn in screen space, ignoring
 * the camera, so call it after the world is drawn.
 */
void DrawFade( IRenderer& inRenderer, graphics::RGBA_Color inColor, float inAmount ) noexcept;

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
    RenderTargetScope( IRenderer const& inRenderer, ITexture& inTarget ) noexcept;

    /**
     * @brief Sends draw calls back to the window.
     */
    ~RenderTargetScope();

    /**
     * @brief Copy construction is disabled.
     */
    RenderTargetScope( RenderTargetScope const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    RenderTargetScope& operator=( RenderTargetScope const& ) = delete;
};

} // namespace asge::video
