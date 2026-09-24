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
 * renderer's zoom straight from Camera::m_Zoom and aims its position at
 * that Transform centered in the current viewport; Camera::m_Smoothing ==
 * 0 snaps there immediately, a positive value eases toward it exponentially
 * (frame-rate independent — see the .cpp) instead of jumping every frame.
 */
void CameraSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

/**
 * @brief Draws every entity that has both a Transform and a Sprite whose
 *        destination rect overlaps the camera's currently visible area
 *        (unless it resolves to screen space -- see below).
 *
 * Transform::m_WorldCoordinates is the sprite's top-left corner (before
 * rotation — see below); m_WorldScale stretches the drawn size — the
 * texture's native size, or Sprite::m_SourceRect's size when set, so a
 * cropped cell of a larger spritesheet is scaled from its own dimensions
 * rather than the whole sheet's. Entities whose Sprite::m_Texture is null
 * are skipped, as is any world-space entity whose destination rect doesn't
 * overlap IRenderer's current camera/viewport at all (see
 * video::VisibleWorldRect) — cheaper than submitting a draw call the
 * backend would just clip away; a screen-space entity (see below) is never
 * culled this way.
 *
 * Transform::m_WorldRotation == 0 (the overwhelming majority of sprites) takes
 * IRenderer's plain Rect-based DrawTexture path; a non-zero rotation
 * instead routes through DrawTextureAffine with corners computed by
 * components::SpriteGetDrawCorners, rotating the sprite around its own
 * center (positive m_Rotation is clockwise on screen).
 *
 * Draw order is sorted, not insertion order, by each entity's resolved
 * components::RenderInfo -- an entity with none of its own sorts as
 * RenderInfo{} (layer 0, no y-sort, world space). World-space entities draw
 * first, screen-space ones last, under a temporary origin/zoom-1 camera
 * restored to the world camera once the last one is drawn; within that, by
 * RenderInfo::m_Layer, then bottom-edge Y when either side opted into
 * RenderInfo::m_YSort, then by sort owner and m_LocalOrder for entities
 * under a components::Hierarchy parent with m_InheritSortFromParent set,
 * and finally by entity index for a stable order.
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
