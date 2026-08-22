#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Velocity.hpp>

namespace asge::game::systems
{

void MovementSystem( ecs::Registry& inRegistry, float inDeltaTime ) noexcept;

}