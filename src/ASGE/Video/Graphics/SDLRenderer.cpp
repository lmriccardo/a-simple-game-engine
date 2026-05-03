#include "SDLRenderer.hpp"

using namespace asge::graphics;

asge::graphics::SDLRenderer::SDLRenderer(SDL_Window *inWindow)
{
    m_Renderer = SDL_CreateRenderer(inWindow, nullptr);
    if (!m_Renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render creation fail : %s\n", SDL_GetError());
    }
}

asge::graphics::SDLRenderer::SDLRenderer(SDLRenderer &&inOther)
: m_Renderer(inOther.m_Renderer)
{
    inOther.m_Renderer = nullptr;
}

SDLRenderer &asge::graphics::SDLRenderer::operator=(SDLRenderer &&inOther)
{
    if ( this != &inOther )
    {
        Destroy(); // First we need to destroy the existing one
        m_Renderer = inOther.m_Renderer;
        inOther.m_Renderer = nullptr;
    }

    return *this;
}

asge::graphics::SDLRenderer::~SDLRenderer()
{
    Destroy();
}

void asge::graphics::SDLRenderer::Clear(RGBA_Color const& inColor) const
{
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render set draw color fail : %s\n", SDL_GetError());
    }

    if (!SDL_RenderClear(m_Renderer))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render clear fail : %s\n", SDL_GetError());
    }
}

void asge::graphics::SDLRenderer::DrawRect(math::Rect const& inRect, RGBA_Color const &inColor) const
{
    DrawRect( inRect.x, inRect.y, inRect.w, inRect.h, inColor );
}

void asge::graphics::SDLRenderer::DrawRect(float x, float y, float w, float h, RGBA_Color const &inColor) const
{
    SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a);
    SDL_FRect rect{ x, y, w, h };
    SDL_RenderRect(m_Renderer, &rect);
}

void asge::graphics::SDLRenderer::Present() const
{
    if (!SDL_RenderPresent(m_Renderer))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render present fail : %s\n", SDL_GetError());
    }
}

bool asge::graphics::SDLRenderer::IsValid() const
{
    return m_Renderer != nullptr;
}

void asge::graphics::SDLRenderer::Destroy()
{
    if (m_Renderer)
    {
        SDL_DestroyRenderer(m_Renderer);
        m_Renderer = nullptr;
    }
}