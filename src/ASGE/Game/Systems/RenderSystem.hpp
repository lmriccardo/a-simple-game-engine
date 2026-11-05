#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

namespace asge::game::systems
{

/**
 * @brief Advances every entity's Animation and writes the current frame into its Sprite.
 *
 * The frame goes into Sprite::m_SourceRect. Entities whose clip or texture is
 * not resolved yet are left untouched. Elapsed time (scaled by m_PlaybackSpeed)
 * steps m_CurrentFrame once per frame duration; at the end it wraps (m_Loop) or
 * clamps and clears m_Playing, emitting events::OnAnimationFinished. A
 * non-positive duration or speed means "not animating".
 */
void AnimationSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

/**
 * @brief Points inRenderer's camera at resources::ActiveCamera's entity, if any.
 *
 * A no-op unless ActiveCamera is a live entity with a Camera and a Transform.
 * Sets the zoom from Camera::m_Zoom and centers on the entity's Sprite
 * destination rect (or the raw Transform point without a resolved Sprite).
 * m_Smoothing == 0 snaps, a positive value eases exponentially.
 */
void CameraSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

/**
 * @brief Draws every entity that has both a Transform and a Sprite, plus the UI widgets.
 *
 * World-space entities are culled when their destination rect misses the
 * visible area. Rotated sprites go through DrawTextureAffine, the rest
 * through plain DrawTexture. Order is by resolved RenderInfo (world space,
 * then screen space under ScreenSpaceCamera; layer, y-sort, entity index).
 * Auto-sized UILabels refresh their UIRect size first.
 */
void RenderSystem( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept;

/**
 * @brief The single per-frame entry point: AnimationSystem, CameraSystem, RenderSystem.
 *
 * Equivalent to calling the three in that order. CameraSystem runs before
 * RenderSystem so this frame's camera move is what gets culled and drawn.
 */
void RenderPipeline(
    ecs::Registry& inRegistry, video::IRenderer& inRenderer, float inDeltaTime ) noexcept;

/**
 * @brief True if inA sorts after inB in RenderSystem's own resolved draw order.
 *
 * That means inA is drawn on top of inB. Uses the same resolved RenderInfo
 * and entity-index tie-break, for tools (a viewport picker, say) that need it
 * outside a draw pass. An entity RenderSystem would not draw never sorts by y.
 */
bool IsDrawnAbove( ecs::Registry const& inRegistry, ecs::Entity inA, ecs::Entity inB ) noexcept;

}
