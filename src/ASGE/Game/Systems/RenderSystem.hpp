#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

namespace asge::game::systems
{

/**
 * @brief Advances every entity's Animation and writes the current frame
 *        into its Sprite::m_SourceRect.
 *
 * Accumulates inDeltaTime into Animation::m_ElapsedTime and steps
 * m_CurrentFrame forward once per whole m_FrameDuration elapsed (a large
 * inDeltaTime can step multiple frames in one call). At the last frame,
 * either wraps to 0 (m_Loop true) or clamps there and clears m_Playing
 * (m_Loop false). Skips entities that aren't playing, have no frames, or
 * have no texture yet; a non-positive m_FrameDuration is treated as "not
 * animating" rather than risking an infinite advance loop.
 */
void AnimationSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

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

/**
 * @brief The single per-frame entry point: AnimationSystem then RenderSystem.
 *
 * Convenience wrapper for callers that want animated sprites without
 * sequencing the two systems themselves — equivalent to calling
 * AnimationSystem(inRegistry, inDeltaTime) followed by
 * RenderSystem(inRegistry, inRenderer).
 */
void RenderPipeline(
    ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

}
