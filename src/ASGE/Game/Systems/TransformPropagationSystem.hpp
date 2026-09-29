#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::systems
{

/**
 * @brief Flattens every Transform's Local (authored) state into its World
 *        (computed) state, composing through components::Hierarchy where
 *        one exists.
 *
 * A root Transform (no Hierarchy, or one with no parent) only recomputes
 * its World fields when its own m_Dirty is set, copying Local straight
 * across. From there, every descendant with both a Transform and a
 * Hierarchy is composed relative to its parent's just-computed World
 * transform, recursively, whenever the child itself is dirty or an
 * ancestor moved this call — a Hierarchy node with no Transform blocks
 * propagation into its own children, since there's no world transform to
 * compose them against. Every Transform actually recomposed has its
 * m_Dirty cleared before returning.
 */
void TransformPropagationSystem( ecs::Registry& inRegistry ) noexcept;

}