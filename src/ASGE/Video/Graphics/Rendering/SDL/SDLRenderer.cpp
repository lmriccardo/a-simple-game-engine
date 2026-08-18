#include "SDLRenderer.hpp"
#include "../RenderError.hpp"
#include "SDLTexture.hpp"

#include <unordered_map>

using namespace asge::video;

asge::video::SDLRenderer::SDLRenderer(SDL_Window *inWindow)
{
    m_Renderer = SDL_CreateRenderer(inWindow, nullptr);
    if (!m_Renderer)
    {
        LogError( make_error_code( errors::RenderError::CreateRendererFailed ), SDL_GetError() );
    }
}

asge::video::SDLRenderer::SDLRenderer(SDLRenderer &&inOther)
: m_Renderer(inOther.m_Renderer)
{
    inOther.m_Renderer = nullptr;
}

SDLRenderer &asge::video::SDLRenderer::operator=(SDLRenderer &&inOther)
{
    if ( this != &inOther )
    {
        Destroy(); // First we need to destroy the existing one
        m_Renderer = inOther.m_Renderer;
        inOther.m_Renderer = nullptr;
    }

    return *this;
}

asge::video::SDLRenderer::~SDLRenderer()
{
    Destroy();
}

void asge::video::SDLRenderer::Clear(graphics::RGBA_Color const& inColor) const
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

void asge::video::SDLRenderer::DrawRect(
    math::Rect const& inRect, graphics::RGBA_Color const &inColor, bool inFill
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

void asge::video::SDLRenderer::DrawLine(
    math::Float2 const &inC1, math::Float2 const &inC2, graphics::RGBA_Color const& inColor
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

void asge::video::SDLRenderer::DrawCircle(
    math::Int2 const& inCenter, int inRadius, graphics::RGBA_Color const &inColor, bool inFill
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

void asge::video::SDLRenderer::DrawTexture(ITexture const &inTexture, math::Rect const &inDestRect) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());
    SDL_FRect dst{ inDestRect.x, inDestRect.y, inDestRect.w, inDestRect.h };
    if ( !SDL_RenderTexture(m_Renderer, texture, nullptr, &dst) )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTexture(ITexture const &inTexture, math::Float2 const &inPosition) const noexcept
{
    auto const size = inTexture.Size();
    DrawTexture(inTexture, math::Rect{
        inPosition.x(), inPosition.y(), static_cast<float>(size.x()), static_cast<float>(size.y())
    });
}

void asge::video::SDLRenderer::DrawTextureTiled(
    ITexture const & inTexture, float inScale, math::Rect const & inDestRect
) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    SDL_FRect dst{ inDestRect.x, inDestRect.y, inDestRect.w, inDestRect.h };
    if ( !SDL_RenderTextureTiled(m_Renderer, texture, nullptr, inScale, &dst) )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTextureAffine(
    ITexture const &inTexture, math::Float2 const &inOrigin, math::Float2 const &inRight, 
    math::Float2 const &inDown) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    SDL_FPoint origin{ inOrigin.x(), inOrigin.y() };
    SDL_FPoint right{ inRight.x(), inRight.y() };
    SDL_FPoint down{ inDown.x(), inDown.y() };

    if (!SDL_RenderTextureAffine(m_Renderer, texture, nullptr, &origin, &right, &down))
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTexture9Grid(
    ITexture const &inTexture, float inLeft, float inRight, float inTop, 
    float inBottom, math::Rect const &inDestRect
) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    SDL_FRect dst{ inDestRect.x, inDestRect.y, inDestRect.w, inDestRect.h };
    auto r = SDL_RenderTexture9Grid(m_Renderer, texture, nullptr,inLeft, inRight, inTop, inBottom, 1.0F, &dst);
    if ( !r )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

std::unique_ptr<ITexture> asge::video::SDLRenderer::CreateTexture(graphics::Image const & inImage) const noexcept
{
    return std::make_unique<SDLTexture>( m_Renderer, inImage );
}

void asge::video::SDLRenderer::Present() const
{
    if (!SDL_RenderPresent(m_Renderer))
    {
        LogError( make_error_code( errors::RenderError::RenderPresentFailed ), SDL_GetError() );
    }
}

bool asge::video::SDLRenderer::IsValid() const
{
    return m_Renderer != nullptr;
}

void asge::video::SDLRenderer::Destroy()
{
    if (m_Renderer)
    {
        SDL_DestroyRenderer(m_Renderer);
        m_Renderer = nullptr;
    }
}
