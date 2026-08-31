#include "Events.hpp"

asge::signals::Signal<asge::ecs::Entity, asge::ecs::Entity> &
asge::game::events::OnTriggerOverlap() noexcept
{
    static signals::Signal<ecs::Entity, ecs::Entity> instance;
    return instance;
}