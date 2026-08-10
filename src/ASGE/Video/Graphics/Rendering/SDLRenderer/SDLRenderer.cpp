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

void asge::graphics::SDLRenderer::DrawRect(
    math::Rect const& inRect, RGBA_Color const &inColor, bool inFill
) const {
    SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a);
    SDL_FRect rect{ inRect.x, inRect.y, inRect.w, inRect.h };
    bool renderResult;

    if ( !inFill ) {
        renderResult = SDL_RenderRect(m_Renderer, &rect);
    } else {
        renderResult = SDL_RenderFillRect(m_Renderer, &rect);
    }

    if (!renderResult)
    {
        LogError( make_error_code( errors::RenderError::RenderRectFailed ), SDL_GetError() );
    }
}

void asge::graphics::SDLRenderer::DrawLine(
    math::Float2 const &inC1, math::Float2 const &inC2, RGBA_Color const& inColor
) const {
    SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a);
    if ( !SDL_RenderLine( m_Renderer, inC1.x(), inC1.y(), inC2.x(), inC2.y() ) )
    {
        LogError( make_error_code( errors::RenderError::RenderLineFailed ), SDL_GetError() );
    }
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
