#pragma once

#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::resources
{

/**
 * @brief Registry-wide pointer at whichever entity systems::CameraSystem
 *        should follow this frame.
 *
 * Set via Registry::SetResource<ActiveCamera>({ entity }); CameraSystem
 * no-ops entirely (leaving IRenderer's camera untouched) while m_Entity is
 * still Entity::Null(), or once set, if that entity doesn't actually carry
 * both a components::Transform and a components::Camera.
 */
struct ActiveCamera
{
    ecs::Entity m_Entity{ ecs::Entity::Null() };
};

}
