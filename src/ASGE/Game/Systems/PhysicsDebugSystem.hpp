#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Collider.hpp>

namespace asge::game::systems
{

/**
 * @brief Draws every Collider's world-space bounds — Rect via DrawRect,
 *        Circle via DrawCircle, both unfilled — color-coded by
 *        ResolutionType (green Solid, yellow Trigger, grey Unknown) so a
 *        mixed scene reads at a glance which colliders actually push out
 *        versus just sense.
 *
 * Purely opt-in: nothing in the physics/render pipeline calls this on its
 * own, a game calls it from its own Render() (typically behind its own
 * debug toggle) the same as any other system.
 */
void DebugDrawColliders( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept;

}
