#pragma once

#include "Renderer.hpp"

namespace asge::video
{

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
