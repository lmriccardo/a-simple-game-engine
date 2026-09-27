#include "SDLRenderer.hpp"
#include "../RenderError.hpp"
#include "SDLTexture.hpp"

#include <cmath>
#include <unordered_map>

using namespace asge::video;

asge::video::SDLRenderer::SDLRenderer(SDL_Window *inWindow)
{
    m_Renderer = SDL_CreateRenderer(inWindow, nullptr);
    if (!m_Renderer)
    {
        LogError( make_error_code( errors::RenderError::CreateRendererFailed ), SDL_GetError() );
    }

    // SDL's own default draw-color blend mode is NONE, which ignores alpha
    // entirely -- DrawRect/DrawLine/DrawCircle's inColor.a would otherwise
    // always render fully opaque regardless of what's passed in.
    if ( m_Renderer && !SDL_SetRenderDrawBlendMode( m_Renderer, SDL_BLENDMODE_BLEND ) )
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    // Default the viewport to the window's own size, so RenderSystem's
    // visible-rect culling (see VisibleWorldRect) has something sane to cull
    // against even for callers that never call SetViewport themselves --
    // matching a zero-size default here would cull every sprite outright.
    int width{0};
    int height{0};
    if ( SDL_GetWindowSize(inWindow, &width, &height) )
    {
        m_Viewport = Viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
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

void asge::video::SDLRenderer::Clear(media::RGBA_Color const& inColor) const
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
    math::Rect const& inRect, media::RGBA_Color const &inColor, bool inFill
) const {
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    math::Rect const screen = TransformRect(m_Camera, inRect);
    SDL_FRect const rect{ screen.m_X, screen.m_Y, screen.m_Width, screen.m_Height };
    bool const renderResult = inFill
        ? SDL_RenderFillRect(m_Renderer, &rect)
        : SDL_RenderRect(m_Renderer, &rect);

    if (!renderResult)
    {
        LogError( make_error_code( errors::RenderError::RenderRectFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawLine(
    math::Float2 const &inC1, math::Float2 const &inC2, media::RGBA_Color const& inColor
) const {
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    math::Float2 const p1 = WorldToScreen(m_Camera, inC1);
    math::Float2 const p2 = WorldToScreen(m_Camera, inC2);

    if ( !SDL_RenderLine( m_Renderer, p1.x(), p1.y(), p2.x(), p2.y() ) )
    {
        LogError( make_error_code( errors::RenderError::RenderLineFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawCircle(
    math::Int2 const& inCenter, int inRadius, media::RGBA_Color const &inColor, bool inFill
) const {
    if (!SDL_SetRenderDrawColor(m_Renderer, inColor.r, inColor.g, inColor.b, inColor.a))
    {
        LogError( make_error_code( errors::RenderError::SetDrawColorFailed ), SDL_GetError() );
    }

    math::Float2 const center = WorldToScreen(
        m_Camera,
        { static_cast<float>(inCenter.x()), static_cast<float>(inCenter.y()) }
    );

    int const radius = static_cast<int>(std::lround(inRadius * m_Camera.m_Zoom));
    math::Int2 const screenCenter{
        static_cast<int>(std::lround(center.x())), static_cast<int>(std::lround(center.y()))
    };

    auto const points = math::MidpointCirclePoints(screenCenter, radius);
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
    math::Rect const screen = TransformRect(m_Camera, inDestRect);
    SDL_FRect dst{ screen.m_X, screen.m_Y, screen.m_Width, screen.m_Height };
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

void asge::video::SDLRenderer::DrawTexture(ITexture const &inTexture, math::Rect const &inSrcRect, math::Rect const &inDestRect) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());
    math::Rect const screenDest = TransformRect(m_Camera, inDestRect);
    SDL_FRect src{ inSrcRect.m_X, inSrcRect.m_Y, inSrcRect.m_Width, inSrcRect.m_Height };
    SDL_FRect dst{ screenDest.m_X, screenDest.m_Y, screenDest.m_Width, screenDest.m_Height };
    if ( !SDL_RenderTexture(m_Renderer, texture, &src, &dst) )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTextureTiled(
    ITexture const & inTexture, float inScale, math::Rect const & inDestRect
) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    math::Rect const screenDest = TransformRect(m_Camera, inDestRect);
    float const tileScale = inScale * m_Camera.m_Zoom;

    SDL_FRect dst{ screenDest.m_X, screenDest.m_Y, screenDest.m_Width, screenDest.m_Height };
    if ( !SDL_RenderTextureTiled(m_Renderer, texture, nullptr, tileScale, &dst) )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTextureAffine(
    ITexture const &inTexture, math::Float2 const &inOrigin, math::Float2 const &inRight, 
    math::Float2 const &inDown) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    math::Float2 const origin = WorldToScreen(m_Camera, inOrigin);
    math::Float2 const right  = WorldToScreen(m_Camera, inRight);
    math::Float2 const down   = WorldToScreen(m_Camera, inDown);

    SDL_FPoint originPt{ origin.x(), origin.y() };
    SDL_FPoint rightPt{ right.x(), right.y() };
    SDL_FPoint downPt{ down.x(), down.y() };

    if (!SDL_RenderTextureAffine(m_Renderer, texture, nullptr, &originPt, &rightPt, &downPt))
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawTextureAffine(
    ITexture const &inTexture, math::Rect const &inSrcRect, math::Float2 const &inOrigin,
    math::Float2 const &inRight, math::Float2 const &inDown) const noexcept
{
    auto* texture = static_cast<SDL_Texture*>(inTexture.NativeHandle());

    math::Float2 const origin = WorldToScreen(m_Camera, inOrigin);
    math::Float2 const right  = WorldToScreen(m_Camera, inRight);
    math::Float2 const down   = WorldToScreen(m_Camera, inDown);

    SDL_FRect src{ inSrcRect.m_X, inSrcRect.m_Y, inSrcRect.m_Width, inSrcRect.m_Height };
    SDL_FPoint originPt{ origin.x(), origin.y() };
    SDL_FPoint rightPt{ right.x(), right.y() };
    SDL_FPoint downPt{ down.x(), down.y() };

    if (!SDL_RenderTextureAffine(m_Renderer, texture, &src, &originPt, &rightPt, &downPt))
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

    math::Rect const screenDest = TransformRect(m_Camera, inDestRect);
    SDL_FRect dst{ screenDest.m_X, screenDest.m_Y, screenDest.m_Width, screenDest.m_Height };
    auto r = SDL_RenderTexture9Grid(m_Renderer, texture, nullptr, inLeft, inRight, inTop, inBottom, m_Camera.m_Zoom, &dst);
    if ( !r )
    {
        LogError( make_error_code( errors::RenderError::RenderTextureFailed ), SDL_GetError() );
    }
}

void asge::video::SDLRenderer::DrawString(
    str::StringView inText, media::Font const &inFont, ITexture &inTexture, 
    math::Float2 const &inPosition, media::RGBA_Color const &inColor
) const noexcept
{
    inTexture.SetColorMod( inColor );
    float penX = inPosition.x();
    float const penY = inPosition.y();

    for ( char c : inText )
    {
        auto glyphResult = inFont.GetGlyph( static_cast<char32_t>(c) );
        if ( !glyphResult ) continue; // Codepoint not baked -- skip

        auto const& glyph = glyphResult.Value();
        math::Rect const destRect{ 
            penX + static_cast<float>( glyph.bearing.x() ),
            penY + static_cast<float>( glyph.bearing.y() ),
            static_cast<float>(glyph.size.x()),
            static_cast<float>(glyph.size.y())
        };

        DrawTexture( inTexture, glyph.uv_rect, destRect );
        penX += static_cast<float>(glyph.advance);
    }
}

std::unique_ptr<ITexture> asge::video::SDLRenderer::CreateTexture(media::Image const & inImage) const noexcept
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

void* asge::video::SDLRenderer::NativeHandle() const noexcept
{
    return m_Renderer;
}

void asge::video::SDLRenderer::SetCamera( Camera const& inCamera )
{
    m_Camera = inCamera;
}

Camera const& asge::video::SDLRenderer::GetCamera() const
{
    return m_Camera;
}

void asge::video::SDLRenderer::SetViewport(Viewport const& inViewport)
{
    m_Viewport = inViewport;

    // Given we are in the SDL Renderer backend which already handles
    // viewport we shall call the SDL_SetRenderViewport function
    SDL_Rect const rect{
        static_cast<int>(inViewport.m_X), static_cast<int>(inViewport.m_Y),
        static_cast<int>(inViewport.m_Width), static_cast<int>(inViewport.m_Height)
    };

    SDL_SetRenderViewport( m_Renderer, &rect );
}

Viewport const& asge::video::SDLRenderer::GetViewport() const
{
    return m_Viewport;
}

void asge::video::SDLRenderer::Destroy()
{
    if (m_Renderer)
    {
        SDL_DestroyRenderer(m_Renderer);
        m_Renderer = nullptr;
    }
}
