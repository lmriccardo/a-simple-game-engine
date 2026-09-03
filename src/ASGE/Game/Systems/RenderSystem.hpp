#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

namespace asge::game::systems
{

/**
 * @brief Draws every entity that has both a Transform and a Sprite.
 *
 * Transform's position is the sprite's top-left corner; scale stretches
 * the drawn size — the texture's native size, or Sprite::m_SourceRect's
 * size when set, so a cropped cell of a larger spritesheet is scaled from
 * its own dimensions rather than the whole sheet's. Entities whose
 * Sprite::m_Texture is null are skipped. Rotation is not applied —
 * IRenderer's Rect-based DrawTexture overloads (the only ones that also
 * support a source rect) don't take one; use DrawTextureAffine directly
 * for a rotating, unclipped sprite.
 *
 * Draw order is sorted, not insertion order: entities are batched by
 * Sprite::m_Layer first (lower layers draw first, so higher layers draw on
 * top); within a layer, entities where either side has Sprite::m_YSort set
 * are further ordered by the sprite's bottom edge (position.y + drawn
 * height) for a 2D painter's-algorithm depth effect; anything still tied
 * falls back to entity index for a stable order.
 */
void RenderSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept;

}
