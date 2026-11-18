#include "Renderer.hpp"

#include <algorithm>
#include <cmath>

using namespace asge::video;

namespace
{

std::uint8_t ScaleChannel( std::uint8_t inValue, float inFactor ) noexcept
{
    return static_cast<std::uint8_t>( std::lround( static_cast<float>( inValue ) * inFactor ) );
}

// Remembers a texture's color and blend mode and puts them back on destruction
class TextureStateGuard
{
private:
    ITexture& m_Texture;                                                  // Texture being temporarily changed
    asge::graphics::RGBA_Color m_Color{};                                 // Color mod to restore
    BlendMode m_Blend{ BlendMode::Alpha };                                // Blend mode to restore

public:
    explicit TextureStateGuard( ITexture& inTexture ) noexcept : m_Texture( inTexture )
    {
        if ( auto color = inTexture.GetColorMod() ) m_Color = color.Value();
        if ( auto blend = inTexture.GetBlendMode() ) m_Blend = blend.Value();
    }

    ~TextureStateGuard()
    {
        m_Texture.SetColorMod( m_Color );
        m_Texture.SetBlendMode( m_Blend );
    }

    TextureStateGuard( TextureStateGuard const& ) = delete;
    TextureStateGuard& operator=( TextureStateGuard const& ) = delete;
};

} // namespace

void asge::video::IRenderer::DrawTextureTinted( ITexture& inTexture,
    math::Rect const& inDestRect, graphics::RGBA_Color inTint ) const noexcept
{
    TextureStateGuard const guard( inTexture );
    inTexture.SetColorMod( inTint );
    DrawTexture( inTexture, inDestRect );
}

void asge::video::IRenderer::DrawTextureGlow( ITexture& inTexture,
    math::Rect const& inDestRect, float inIntensity, graphics::RGBA_Color inColor ) const noexcept
{
    float const intensity = std::clamp( inIntensity, 0.0f, 1.0f );

    // Additive blending adds source * alpha, so scaling alpha scales the glow
    TextureStateGuard const guard( inTexture );
    inTexture.SetBlendMode( BlendMode::Additive );
    inTexture.SetColorMod( { inColor.r, inColor.g, inColor.b, ScaleChannel( inColor.a, intensity ) } );
    DrawTexture( inTexture, inDestRect );
}

void asge::video::IRenderer::DrawFade( graphics::RGBA_Color inColor, float inAmount ) const noexcept
{
    float const amount = std::clamp( inAmount, 0.0f, 1.0f );
    graphics::RGBA_Color const color{ inColor.r, inColor.g, inColor.b, ScaleChannel( inColor.a, amount ) };

    // The world rect the camera maps onto the viewport, so the overlay fills the screen at any camera
    DrawRect( VisibleWorldRect( GetCamera(), GetViewport() ), color, true );
}
