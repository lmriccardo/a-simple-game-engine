#pragma once

#include "../../Renderer.hpp"
#include <string>
#include <SDL3/SDL.h>

namespace asge::video
{

/**
 * A owing wrapper around SDL_Render
 */
class SDLRenderer final : public IRenderer
{
private:
    SDL_Renderer * m_Renderer{nullptr};

public:
    SDLRenderer(SDL_Window* inWindow);
    SDLRenderer(SDLRenderer&& inOther);
    SDLRenderer& operator=(SDLRenderer&& inOther);

    virtual ~SDLRenderer();

    // Renderer is not copyiable at all
    SDLRenderer(SDLRenderer const& inOther) = delete;
    SDLRenderer& operator=(SDLRenderer const& inOther) = delete;

    void Clear(graphics::RGBA_Color const& inColor) const override;
    void DrawRect(math::Rect const& inRect, graphics::RGBA_Color const& inColor, bool inFill) const override;
    void DrawLine(math::Float2 const& inC1, math::Float2 const& inC2, graphics::RGBA_Color const& inColor) const override;
    void DrawCircle(math::Int2 const& inCenter, int inRadius, graphics::RGBA_Color const& inColor, bool inFill) const override;

    void Present() const override;
    bool IsValid() const override;

    // Destroy procedure for the renderer
    void Destroy();
};

} // namespace asge::video
