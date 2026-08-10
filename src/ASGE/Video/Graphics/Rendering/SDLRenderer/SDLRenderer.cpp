#include "SDLRenderer.hpp"
#include "../RenderError.hpp"

#include <unordered_map>

using namespace asge::graphics;

asge::graphics::SDLRenderer::SDLRenderer(SDL_Window *inWindow)
{
    m_Renderer = SDL_CreateRenderer(inWindow, nullptr);
    if (!m_Renderer)
    {
        LogError( make_error_code( errors::RenderError::CreateRendererFailed ), SDL_GetError() );
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
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    if (!SDL_RenderClear(m_Renderer))
    {
        LogError( make_error_code( errors::RenderError::RenderClearFailed ), SDL_GetError() );
    }
}

void asge::graphics::SDLRenderer::DrawRect(
    math::Rect const& inRect, RGBA_Color const &inColor, bool inFill
) const {
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

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
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    if ( !SDL_RenderLine( m_Renderer, inC1.x(), inC1.y(), inC2.x(), inC2.y() ) )
    {
        LogError( make_error_code( errors::RenderError::RenderLineFailed ), SDL_GetError() );
    }
}

void asge::graphics::SDLRenderer::DrawCircle(
    math::Int2 const& inCenter, int inRadius, RGBA_Color const &inColor, bool inFill
) const {
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    auto const points = math::MidpointCirclePoints(inCenter, inRadius);
    bool renderResult;

    if ( !inFill )
    {
        std::vector<SDL_FPoint> sdlPoints;
        sdlPoints.reserve( points.size() );
        for ( auto const& point : points )
        {
            sdlPoints.push_back({ static_cast<float>(point.x()), static_cast<float>(point.y()) });
        }

        renderResult = SDL_RenderPoints(m_Renderer, sdlPoints.data(), static_cast<int>(sdlPoints.size()));
    }
    else
    {
        // Group the outline points by row and fill each row between its
        // leftmost and rightmost point, reusing the same point set.
        std::unordered_map<int, std::pair<int, int>> rowSpans;
        for ( auto const& point : points )
        {
            auto [it, inserted] = rowSpans.try_emplace( point.y(), point.x(), point.x() );
            if ( !inserted )
            {
                it->second.first  = point.x() < it->second.first  ? point.x() : it->second.first;
                it->second.second = point.x() > it->second.second ? point.x() : it->second.second;
            }
        }

        renderResult = true;
        for ( auto const& [y, span] : rowSpans )
        {
            renderResult &= SDL_RenderLine(
                m_Renderer, static_cast<float>(span.first), static_cast<float>(y),
                static_cast<float>(span.second), static_cast<float>(y)
            );
        }
    }

    if (!renderResult)
    {
        LogError( make_error_code( errors::RenderError::RenderCircleFailed ), SDL_GetError() );
    }
}

void asge::graphics::SDLRenderer::Present() const
{
    if (!SDL_RenderPresent(m_Renderer))
    {
        LogError( make_error_code( errors::RenderError::RenderPresentFailed ), SDL_GetError() );
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
