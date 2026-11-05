#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Collider.hpp>

namespace asge::game::systems
{

/**
 * @brief Draws every Collider's world-space bounds as unfilled outlines.
 *
 * Rect via DrawRect and Circle via DrawCircle, colored by ResolutionType
 * (green Solid, yellow Trigger, grey Unknown). Opt-in: nothing calls it
 * automatically, a game calls it from its own Render().
 */
void DebugDrawColliders( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept;

}
