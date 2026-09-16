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

/** @brief inSprite's on-screen destination rect at inT's position/scale, or nullopt if it has no texture yet. */
std::optional<math::Rect> SpriteGetDstRect( Sprite const& inSprite, Transform const& inT ) noexcept;

/** @brief The three corners IRenderer::DrawTextureAffine maps a texture's (0,0)/(w,0)/(0,h) onto. */
struct SpriteDrawCorners
{
    math::Float2 m_Origin; // texture's top-left (0,0) maps here
    math::Float2 m_Right;  // texture's top-right (w,0) maps here
    math::Float2 m_Down;   // texture's bottom-left (0,h) maps here
};

/**
 * @brief inDstRect's own corners (as SpriteGetDstRect returns), rotated
 *        inRotationRadians around inDstRect's center.
 *
 * Positive inRotationRadians rotates clockwise on screen (screen-space Y
 * grows downward); inRotationRadians == 0 reproduces inDstRect's unrotated
 * corners exactly. Feeds IRenderer::DrawTextureAffine, the only DrawTexture*
 * overload that can express rotation — see RenderSystem, which switches to
 * it whenever a drawn entity's Transform::m_Rotation is non-zero.
 */
SpriteDrawCorners SpriteGetDrawCorners( math::Rect const& inDstRect, float inRotationRadians ) noexcept;

template<>
struct Serializer<Sprite>
{
    using T = Sprite;

    /** @brief The subtable name ToToml/FromToml agree on — see Serializer<Transform>::kTableName. */
    static constexpr str::StringView kTableName = "Sprite";

    static void ToToml( Sprite inSprite, asge::config::toml::TOMLTableView inTview ) noexcept;
    static T FromToml( asge::config::toml::TOMLTableView inEnttView ) noexcept;
};

}
