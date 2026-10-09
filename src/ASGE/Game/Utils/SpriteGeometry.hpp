#pragma once

#include <optional>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Transform.hpp>

namespace asge::game::utils
{

/** @brief inSprite's on-screen destination rect at inT's world position/scale, or nullopt if it has no texture yet. */
std::optional<math::Rect> SpriteGetDstRect( components::Sprite const& inSprite, components::Transform const& inT ) noexcept;

/** @brief Like SpriteGetDstRect but from inT's m_Local* values: its top-left and size (native size * local scale). */
std::optional<math::Rect> SpriteGetLocalRect( components::Sprite const& inSprite, components::Transform const& inT ) noexcept;

/**
 * @brief Sets ioT's local scale to inNewScale, moving its local position so the sprite's
 *        centre stays put, and marks it dirty. Does nothing if inSprite has no texture yet.
 */
void SpriteScaleAroundCenter( components::Sprite const& inSprite, components::Transform& ioT, math::Float2 inNewScale ) noexcept;

}
