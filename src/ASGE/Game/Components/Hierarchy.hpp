#pragma once

#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::components
{

struct Hierarchy
{
    asge::ecs::Entity m_Parent      = asge::ecs::Entity::Null();
    asge::ecs::Entity m_FirstChild  = asge::ecs::Entity::Null();
    asge::ecs::Entity m_LastChild   = asge::ecs::Entity::Null();
    asge::ecs::Entity m_NextSibling = asge::ecs::Entity::Null();
    asge::ecs::Entity m_PrevSibling = asge::ecs::Entity::Null();
};

}