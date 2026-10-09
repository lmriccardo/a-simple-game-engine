#include "Sprite.hpp"

#include <cmath>
#include <cstdint>

asge::game::components::SpriteDrawCorners asge::game::components::SpriteGetDrawCorners(
    math::Rect const& inDstRect, float inRotationRadians ) noexcept
{
    float const centerX = inDstRect.m_X + inDstRect.m_Width  * 0.5f;
    float const centerY = inDstRect.m_Y + inDstRect.m_Height * 0.5f;
    float const halfW = inDstRect.m_Width  * 0.5f;
    float const halfH = inDstRect.m_Height * 0.5f;

    float const cosR = std::cos( inRotationRadians );
    float const sinR = std::sin( inRotationRadians );

    // Rotates a point given relative to the rect's own center; screen-space
    // Y grows downward, so this reads as a clockwise rotation on screen.
    auto rotate = [&]( float inLocalX, float inLocalY ) noexcept -> math::Float2
    {
        return math::Float2{
            centerX + inLocalX * cosR - inLocalY * sinR,
            centerY + inLocalX * sinR + inLocalY * cosR
        };
    };

    return SpriteDrawCorners{
        rotate( -halfW, -halfH ), // origin: top-left
        rotate(  halfW, -halfH ), // right:  top-right
        rotate( -halfW,  halfH )  // down:   bottom-left
    };
}

void asge::game::scene::Serializer<asge::game::components::Sprite>::ToToml(
    components::Sprite inSprite, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto sprite = inTview.Table(std::string(kTableName));
    sprite.Set<std::string>("m_VirtualPath", inSprite.m_VirtualPath);
    sprite.Set("m_Tint", static_cast<std::int64_t>(graphics::RGBATo32A( inSprite.m_Tint )));

    if ( inSprite.m_SourceRect )
    {
        sprite.Table("SourceRect")
              .Set("x", inSprite.m_SourceRect->m_X)
              .Set("y", inSprite.m_SourceRect->m_Y)
              .Set("w", inSprite.m_SourceRect->m_Width)
              .Set("h", inSprite.m_SourceRect->m_Height);
    }
}

asge::game::components::Sprite asge::game::scene::Serializer<asge::game::components::Sprite>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto sprite = inEnttView.Table(std::string(kTableName));

    components::Sprite result{};
    result.m_VirtualPath = sprite.Get<std::string>("m_VirtualPath", std::string{});

    auto const dTint = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_Tint ) );
    result.m_Tint = graphics::C32AToRGBA( static_cast<graphics::Color32A>( sprite.Get("m_Tint", dTint) ) );

    if ( sprite.HasTable("SourceRect") )
    {
        auto rect = sprite.Table("SourceRect");
        result.m_SourceRect = math::Rect{
            rect.Get("x", 0.0f),
            rect.Get("y", 0.0f),
            rect.Get("w", 0.0f),
            rect.Get("h", 0.0f)
        };
    }

    return result;
}
