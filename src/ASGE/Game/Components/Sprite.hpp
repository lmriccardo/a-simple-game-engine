#pragma once

#include <optional>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Math/Math.hpp>

namespace asge::game::components
{

/**
 * @brief A drawable texture reference, drawn each frame via RenderSystem.
 *
 * Non-owning: m_Texture must outlive every entity holding a Sprite that
 * points to it (typically owned by the IGame itself, since a renderer —
 * and so a texture — cannot be created before one exists).
 */
struct Sprite
{
    video::ITexture* m_Texture{nullptr};      // Non-owning; nullptr means "not drawn"
    std::optional<math::Rect> m_SourceRect{}; // Sub-region to draw; nullopt = whole texture
};

}
