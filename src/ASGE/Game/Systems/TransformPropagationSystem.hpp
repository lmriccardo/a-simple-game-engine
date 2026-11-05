#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::systems
{

/**
 * @brief Flattens every Transform's Local state into its World state.
 *
 * Composes through components::Hierarchy. A root only recomputes when its own
 * m_Dirty is set; a descendant is recomposed under its parent's fresh World
 * transform when it or an ancestor is dirty. A Hierarchy node without a
 * Transform stops propagation. m_Dirty is cleared on every recomposed Transform.
 */
void TransformPropagationSystem( ecs::Registry& inRegistry ) noexcept;

}