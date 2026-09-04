#pragma once

#include <optional>
#include <string>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include "Transform.hpp"
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief A drawable texture reference, drawn each frame via RenderSystem.
 *
 * Non-owning: m_Texture must outlive every entity holding a Sprite that
 * points to it — in practice asset::AssetManager, since a renderer (and so
 * a texture) can't be created before one exists, so nothing can own the
 * texture at component-construction time. asset::AssetManager::
 * ResolveAssets is what actually creates it and keeps it alive.
 *
 * m_VirtualPath is what actually round-trips through TOML — m_Texture is a
 * runtime-only pointer that can't be serialized, so ToToml/FromToml carry
 * the path it was loaded from instead. FromToml leaves m_Texture null;
 * asset::AssetManager::ResolveAssets is what resolves m_VirtualPath back
 * into a live texture for every Sprite that still needs one.
 */
struct Sprite
{
    video::ITexture*            m_Texture{nullptr}; // Non-owning; nullptr means "not drawn"
    std::optional<math::Rect>   m_SourceRect{};     // Sub-region to draw; nullopt = whole texture
    std::string                 m_VirtualPath{};    // VFS path m_Texture was (or will be) loaded from
    int                         m_Layer{0};         // Draw-order bucket; higher layers draw on top
    bool                        m_YSort{false};     // Opt into sorting by bottom-edge Y within the layer
};

inline std::optional<math::Rect> 
SpriteGetDstRect( Sprite const& inSprite, Transform const& inT ) noexcept
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

template<>
struct Serializer<Sprite>
{
    using T = Sprite;

    /** @brief The subtable name ToToml/FromToml agree on — see Serializer<Transform>::kTableName. */
    static constexpr str::StringView kTableName = "Sprite";

    static void ToToml(
        Sprite inSprite, asge::config::toml::TOMLTableView inTview
    ) noexcept {
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

    static T FromToml( asge::config::toml::TOMLTableView inEnttView ) noexcept
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
};

}
