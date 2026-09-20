#include "Sprite.hpp"

#include <cmath>

std::optional<asge::math::Rect> asge::game::components::SpriteGetDstRect(
    Sprite const& inSprite, Transform const& inT ) noexcept
{
    if ( !inSprite.m_Texture ) return std::nullopt;

    auto const& texture = *inSprite.m_Texture;
    auto const& srcRect = inSprite.m_SourceRect;
    float srcW{}, srcH{};

    if ( srcRect.has_value() )
    {
        srcW = srcRect->m_Width;
        srcH = srcRect->m_Height;
    }
    else
    {
        math::Int2 const texSize = texture.Size();
        srcW = static_cast<float>(texSize.x());
        srcH = static_cast<float>(texSize.y());
    }

    return math::Rect{
        inT.m_WorldCoordinates.x(), inT.m_WorldCoordinates.y(),
        srcW * inT.m_WorldScale.x(), srcH * inT.m_WorldScale.y()
    };
}

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

    if ( inSprite.m_SourceRect )
    {
        sprite.Table("SourceRect")
              .Set("x", inSprite.m_SourceRect->m_X)
              .Set("y", inSprite.m_SourceRect->m_Y)
              .Set("w", inSprite.m_SourceRect->m_Width)
              .Set("h", inSprite.m_SourceRect->m_Height);
    }

    sprite.Set("m_Layer", inSprite.m_Layer);
    sprite.Set("m_YSort", inSprite.m_YSort);
}

asge::game::components::Sprite asge::game::scene::Serializer<asge::game::components::Sprite>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto sprite = inEnttView.Table(std::string(kTableName));

    components::Sprite result{};
    result.m_VirtualPath = sprite.Get<std::string>("m_VirtualPath", std::string{});

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

    result.m_Layer = sprite.Get( "m_Layer", int{0} );
    result.m_YSort = sprite.Get( "m_YSort", false );

    return result;
}
