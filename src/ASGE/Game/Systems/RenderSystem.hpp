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
 * Skipped entirely until Animation::m_Clip is resolved (see
 * asset::AssetManager::ResolveAssets) — an entity whose clip hasn't loaded
 * yet, or has no Sprite::m_Texture yet either, is left untouched rather
 * than animated against an empty frame list. Once resolved, accumulates
 * inDeltaTime into Animation::m_ElapsedTime and steps m_CurrentFrame
 * forward once per whole m_FrameDuration elapsed (a large inDeltaTime can
 * step multiple frames in one call). At the last frame, either wraps to 0
 * (m_Loop true) or clamps there and clears m_Playing (m_Loop false). A
 * non-positive m_FrameDuration is treated as "not animating" rather than
 * risking an infinite advance loop.
 */
void AnimationSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Points inRenderer's camera at resources::ActiveCamera's entity, if any.
 *
 * A no-op (leaving inRenderer's camera exactly as it was) unless
 * ActiveCamera is set to a live entity that carries both a
 * components::Camera and a components::Transform. When it does, sets the
 * renderer's zoom straight from Camera::m_Zoom and centers the viewport on
 * that entity's own Sprite::SpriteGetDstRect midpoint, if it has a Sprite
 * with a resolved texture — not the raw Transform::m_X/m_Y, which is that
 * rect's top-left corner (see SpriteGetDstRect), not its visual middle;
 * centering on the corner would render the sprite offset down-right from
 * screen-center rather than actually centered. Falls back to the raw
 * Transform point for an entity with no Sprite (or an unresolved one) to
 * follow instead. Camera::m_Smoothing == 0 snaps there immediately, a
 * positive value eases toward it exponentially (frame-rate independent —
 * see the .cpp) instead of jumping every frame.
 */
void CameraSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

/**
 * @brief Draws every entity that has both a Transform and a Sprite whose
 *        destination rect overlaps the camera's currently visible area.
 *
 * Transform's position is the sprite's top-left corner (before rotation —
 * see below); scale stretches the drawn size — the texture's native size,
 * or Sprite::m_SourceRect's size when set, so a cropped cell of a larger
 * spritesheet is scaled from its own dimensions rather than the whole
 * sheet's. Entities whose Sprite::m_Texture is null are skipped, as is any
 * entity whose destination rect doesn't overlap IRenderer's current
 * camera/viewport at all (see video::VisibleWorldRect) — cheaper than
 * submitting a draw call the backend would just clip away.
 *
 * Transform::m_Rotation == 0 (the overwhelming majority of sprites) takes
 * IRenderer's plain Rect-based DrawTexture path; a non-zero rotation
 * instead routes through DrawTextureAffine with corners computed by
 * components::SpriteGetDrawCorners, rotating the sprite around its own
 * center (positive m_Rotation is clockwise on screen).
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
 * @brief The single per-frame entry point: AnimationSystem, then
 *        CameraSystem, then RenderSystem.
 *
 * Convenience wrapper for callers that want animated, camera-followed
 * sprites without sequencing the three systems themselves — equivalent to
 * calling AnimationSystem(inRegistry, inDeltaTime), then
 * CameraSystem(inRegistry, inRenderer, inDeltaTime), then
 * RenderSystem(inRegistry, inRenderer). CameraSystem must run before
 * RenderSystem so this frame's camera move is what RenderSystem culls and
 * draws against, not last frame's.
 */
void RenderPipeline(
    ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

}
