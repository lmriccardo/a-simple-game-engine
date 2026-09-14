#include "Sprite.hpp"

std::optional<asge::math::Rect> asge::game::components::SpriteGetDstRect(
    Sprite const& inSprite, Transform const& inT ) noexcept
{
    if ( !inSprite.m_Texture ) return std::nullopt;

    auto const& texture = *inSprite.m_Texture;
    auto const& srcRect = inSprite.m_SourceRect;
    float srcW{}, srcH{};

    if ( srcRect.has_value() )
    {
        srcW = srcRect->w;
        srcH = srcRect->h;
    }
    else
    {
        math::Int2 const texSize = texture.Size();
        srcW = static_cast<float>(texSize.x());
        srcH = static_cast<float>(texSize.y());
    }

    return math::Rect{
        inT.m_X, inT.m_Y, srcW * inT.m_ScaleX, srcH * inT.m_ScaleY
    };
}

void asge::game::components::Serializer<asge::game::components::Sprite>::ToToml(
    Sprite inSprite, asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto sprite = inTview.Table(std::string(kTableName));
    sprite.Set<std::string>("m_VirtualPath", inSprite.m_VirtualPath);

    if ( inSprite.m_SourceRect )
    {
        sprite.Table("SourceRect")
              .Set("x", inSprite.m_SourceRect->x)
              .Set("y", inSprite.m_SourceRect->y)
              .Set("w", inSprite.m_SourceRect->w)
              .Set("h", inSprite.m_SourceRect->h);
    }

    sprite.Set("m_Layer", inSprite.m_Layer);
    sprite.Set("m_YSort", inSprite.m_YSort);
}

asge::game::components::Sprite asge::game::components::Serializer<asge::game::components::Sprite>::FromToml(
    asge::config::toml::TOMLTableView inEnttView ) noexcept
{
    auto sprite = inEnttView.Table(std::string(kTableName));

    Sprite result{};
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
