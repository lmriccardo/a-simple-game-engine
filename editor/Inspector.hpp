#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

// Editor-only inspector UI -- lives here, not in src/ASGE/Game/Components/,
// since it depends on ImGui and the engine library itself must not.

/**
 * @brief Lists every entity in inRegistry; clicking one sets ioSelected.
 */
void DrawEntityListPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected ) noexcept;

/**
 * @brief Draws one section per currently-serializable component type
 *        inSelected actually has, each editing the live Registry component
 *        directly (no intermediate copy) -- a fixed, explicit list of known
 *        types (Registry::HasComponent<T> per type), not a generic
 *        reflection system. A no-op if inSelected is Entity::Null().
 */
void DrawInspectorPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected ) noexcept;
