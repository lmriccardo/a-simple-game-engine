#pragma once

#include <optional>
#include <string>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
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
 *
 * m_SourceRect is owned by components::Animation only while it is playing:
 * AnimationSystem overwrites it every frame then, and restores the crop it
 * found (a hand-set rect, or whole-texture nullopt) once the Animation stops
 * or its clip is cleared. While nothing plays, the Sprite's own m_SourceRect
 * is what is drawn.
 */
struct Sprite
{
    video::ITexture*            m_Texture{nullptr}; // Not serialized -- non-owning; nullptr means "not drawn"
    std::optional<math::Rect>   m_SourceRect{};     // Serialized. Sub-region to draw; nullopt = whole texture
    std::string                 m_VirtualPath{};    // Serialized. VFS path m_Texture was (or will be) loaded from
    graphics::RGBA_Color        m_Tint{};           // Serialized. Color modulation; the alpha channel fades the sprite (white = untouched)
    std::string                 m_ResolvedVirtualPath{}; // Runtime-only: the path m_Texture was actually last resolved from
};

/**
 * @brief The three corners IRenderer::DrawTextureAffine maps a texture onto.
 *
 * They are the images of the texture's (0,0), (w,0) and (0,h) points, which
 * is enough to describe any rotation or scale.
 */
struct SpriteDrawCorners
{
    math::Float2 m_Origin; // texture's top-left (0,0) maps here
    math::Float2 m_Right;  // texture's top-right (w,0) maps here
    math::Float2 m_Down;   // texture's bottom-left (0,h) maps here
};

/**
 * @brief Returns inDstRect's corners, rotated around its center.
 *
 * inDstRect is the rect utils::SpriteGetDstRect returns. Positive
 * inRotationRadians is clockwise on screen, and 0 reproduces the unrotated
 * corners. Feeds IRenderer::DrawTextureAffine, which RenderSystem uses when
 * Transform::m_WorldRotation is non-zero.
 */
SpriteDrawCorners SpriteGetDrawCorners( math::Rect const& inDstRect, float inRotationRadians ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::Sprite>
{
    using T = components::Sprite;

    static constexpr str::StringView kTableName = "Sprite"; // Name of the TOML subtable (de)serialization uses

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            components::Sprite inSprite,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
