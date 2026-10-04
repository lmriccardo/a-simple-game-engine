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
 * first, screen-space ones last, under a temporary camera (origin/zoom-1 by
 * default, or resources::ScreenSpaceCamera's own value if that resource is
 * set -- see its own doc comment for why) restored to the world camera once
 * the last one is drawn; within that, by RenderInfo::m_Layer, then
 * bottom-edge Y (shifted by RenderInfo::m_SortOffsetY) when either side opted into RenderInfo::m_YSort, then by
 * sort owner and m_LocalOrder for entities under a components::Hierarchy
 * parent with m_InheritSortFromParent set, and finally by entity index for
 * a stable order.
 *
 * Before any of that, a resolved components::UILabel with m_AutoSize writes
 * its own Font::Measure(m_Text) straight into its sibling components::UIRect's
 * m_Size, so this frame's destination rect already reflects the current text
 * rather than whatever size the entity happened to be created with.
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

/**
 * @brief True if inA sorts after inB in RenderSystem's own resolved draw
 *        order -- i.e. inA is drawn on top of inB -- without re-deriving
 *        that order from scratch: same resolved RenderInfo (screen-space
 *        last, then layer, then y-sort's bottom edge, then sort owner/
 *        local order/inheritance depth), tie-broken by entity index.
 *        For a tool (a viewport picker, say) that needs "which of these
 *        two would end up on top" outside of an actual draw pass.
 *
 * An entity with no Sprite/UIRect (nothing RenderSystem would draw) still
 * resolves a layer/screen-space position -- it just never sorts by y, since
 * there's no dst rect to compute a bottom edge from.
 */
bool IsDrawnAbove( ecs::Registry const& inRegistry, ecs::Entity inA, ecs::Entity inB ) noexcept;

}
