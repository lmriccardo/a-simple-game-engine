#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

// Editor-only inspector UI -- lives here, not in src/ASGE/Game/Components/,
// since it depends on ImGui and the engine library itself must not.

// Shared right-side panel layout constants, so Scene (main.cpp)/Entities/
// Inspector stack vertically along the same right-aligned column instead of
// ImGui's default cascade-on-top-of-each-other for windows with no
// explicit position.
constexpr float kEditorPanelWidth = 300.0f;
constexpr float kEditorPanelRightMargin = 10.0f;

/**
 * @brief Lists every entity in inRegistry; clicking one sets ioSelected.
 * @return True the one frame a "Create" click happened -- the caller (which
 *         owns the SceneManager this Registry belongs to, unlike this file)
 *         performs the actual Registry::CreateEntity() + SceneId tagging.
 */
bool DrawEntityListPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected ) noexcept;

/** @brief Which entity-lifecycle action (if any) DrawInspectorPanel's buttons requested this frame. */
enum class EntityAction { None, Delete, Duplicate };

/**
 * @brief Draws one section per currently-serializable component type
 *        inSelected actually has, each editing the live Registry component
 *        directly (no intermediate copy) -- a fixed, explicit list of known
 *        types (Registry::HasComponent<T> per type), not a generic
 *        reflection system. A no-op if inSelected is Entity::Null().
 * @return Which lifecycle action (if any) its Delete/Duplicate buttons
 *         requested -- performed by the caller, same reasoning as
 *         DrawEntityListPanel's Create signal.
 */
EntityAction DrawInspectorPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected ) noexcept;

/**
 * @brief Restarts the "Entity #N" fallback numbering (see GetEntityLabel)
 *        at 0 -- call when starting a fresh scene (File > New), so a new
 *        scene's unnamed entities count up from 0 instead of continuing
 *        wherever the previous scene's counter left off.
 */
void ResetEntityDisplayIds() noexcept;
