#pragma once

#include <optional>
#include <ASGE/Core/Math/Math.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Transform.hpp>

namespace asge::game::utils
{

/**
 * @brief Returns the sprite's on-screen destination rect.
 *
 * Uses inT's world position and scale, with the texture's native size (or the
 * source rect's size when set). Gives nullopt if the sprite has no texture yet.
 */
std::optional<math::Rect> SpriteGetDstRect( components::Sprite const& inSprite, components::Transform const& inT ) noexcept;

/**
 * @brief Like SpriteGetDstRect but from inT's m_Local* values.
 *
 * Its top-left and size (native size * local scale).
 */
std::optional<math::Rect> SpriteGetLocalRect( components::Sprite const& inSprite, components::Transform const& inT ) noexcept;

/**
 * @brief Sets ioT's local scale to inNewScale around the sprite's center.
 *
 * Moves the local position so the sprite's centre stays put, and marks the
 * transform dirty. Does nothing if inSprite has no texture yet.
 */
void SpriteScaleAroundCenter( components::Sprite const& inSprite, components::Transform& ioT, math::Float2 inNewScale ) noexcept;

}
