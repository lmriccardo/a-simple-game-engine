#pragma once

#include <optional>
#include <string>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief A drawable texture reference, drawn each frame via RenderSystem.
 *
 * Non-owning: m_Texture must outlive every entity holding a Sprite that
 * points to it (typically owned by the IGame itself, since a renderer —
 * and so a texture — cannot be created before one exists).
 *
 * m_VirtualPath is what actually round-trips through TOML — m_Texture is a
 * runtime-only pointer that can't be serialized, so ToToml/FromToml carry
 * the path it was loaded from instead. FromToml leaves m_Texture null;
 * resolving it back into a live texture (via AssetManager + IRenderer) is
 * the caller's job, the same deferred-load step ecs_demo already does for
 * freshly-spawned sprites.
 */
struct Sprite
{
    video::ITexture* m_Texture{nullptr};      // Non-owning; nullptr means "not drawn"
    std::optional<math::Rect> m_SourceRect{}; // Sub-region to draw; nullopt = whole texture
    std::string m_VirtualPath{};              // VFS path m_Texture was (or will be) loaded from
};

template<>
struct Serializer<Sprite>
{
    using T = Sprite;

    /** @brief The subtable name ToToml/FromToml agree on — see Serializer<Transform>::kTableName. */
    static constexpr str::StringView kTableName = "Sprite";

    static void ToToml(
        Sprite inSprite, asge::config::TOMLTableView inTview
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
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
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

        return result;
    }
};

}
