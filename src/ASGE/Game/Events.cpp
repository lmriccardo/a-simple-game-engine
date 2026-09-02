#include "Events.hpp"

asge::signals::Signal<asge::ecs::Entity, asge::ecs::Entity> &
asge::game::events::OnCollisionTriggerEnter() noexcept
{
    static signals::Signal<ecs::Entity, ecs::Entity> instance;
    return instance;
}

asge::signals::Signal<asge::ecs::Entity, asge::ecs::Entity> &
asge::game::events::OnCollisionTriggerExit() noexcept
{
    static signals::Signal<ecs::Entity, ecs::Entity> instance;
    return instance;
}

asge::signals::Signal<asge::ecs::Entity, asge::ecs::Entity> &
asge::game::events::OnCollisionTriggerStay() noexcept
{
    static signals::Signal<ecs::Entity, ecs::Entity> instance;
    return instance;
}