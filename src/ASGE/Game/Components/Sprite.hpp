#pragma once

#include <optional>
#include <string>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Math/Math.hpp>
#include "Transform.hpp"
#include <ASGE/Game/Scene/Serialize.hpp>

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
 * m_VirtualPath and m_SourceRect are what actually round-trip through TOML
 * — m_Texture is a runtime-only pointer that can't be serialized, so
 * ToToml/FromToml carry the path it was loaded from instead. FromToml
 * leaves m_Texture null; asset::AssetManager::ResolveAssets is what
 * resolves m_VirtualPath back into a live texture for every Sprite that
 * still needs one. Draw order isn't a Sprite concern -- see
 * components::RenderInfo for layer/y-sort/screen-space.
 */
struct Sprite
{
    video::ITexture*            m_Texture{nullptr}; // Not serialized -- non-owning; nullptr means "not drawn"
    std::optional<math::Rect>   m_SourceRect{};     // Serialized. Sub-region to draw; nullopt = whole texture
    std::string                 m_VirtualPath{};    // Serialized. VFS path m_Texture was (or will be) loaded from
    std::string                 m_ResolvedVirtualPath{}; // Runtime-only: the path m_Texture was actually last resolved from
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
 * it whenever a drawn entity's Transform::m_WorldRotation is non-zero.
 */
SpriteDrawCorners SpriteGetDrawCorners( math::Rect const& inDstRect, float inRotationRadians ) noexcept;

}

namespace asge::game::scene
{

/** @brief Round-trips m_VirtualPath and m_SourceRect only -- m_Texture is runtime-only and always comes back null from FromToml (see Sprite's own doc comment). */
template<>
struct Serializer<components::Sprite>
{
    using T = components::Sprite;

    static constexpr str::StringView kTableName = "Sprite";

    static void ToToml(
                            components::Sprite inSprite,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
