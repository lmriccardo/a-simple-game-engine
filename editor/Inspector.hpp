#pragma once

#include <ASGE/Core/ECS/Registry.hpp>

#include <string>
#include <vector>

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

/**
 * @brief Which entity-lifecycle action (if any) DrawInspectorPanel's buttons
 *        requested this frame. ComponentsChanged covers Add/Remove Component
 *        -- the caller uses it to know when to re-run AssetManager::
 *        ResolveAssets (a newly-added Sprite/Animation/AudioSource has
 *        nothing resolved yet) without having to do it unconditionally every
 *        frame, which would re-log the same failure for anything that stays
 *        unresolved.
 */
enum class EntityAction { None, Delete, Duplicate, ComponentsChanged };

/**
 * @brief Draws one section per currently-serializable component type
 *        inSelected actually has, each editing the live Registry component
 *        directly (no intermediate copy) -- a fixed, explicit list of known
 *        types (Registry::HasComponent<T> per type), not a generic
 *        reflection system. A no-op if inSelected is Entity::Null().
 *
 * @param inKnownTextures Every texture virtual path the editor currently
 *        knows about (see editor/AssetBrowser.hpp's KnownTexturePaths) --
 *        adding a Sprite requires picking one of these up front instead of
 *        starting with a blank, unresolved m_VirtualPath the user would
 *        have to fill in by hand anyway.
 * @return Which lifecycle action (if any) its buttons requested -- performed
 *         by the caller, same reasoning as DrawEntityListPanel's Create
 *         signal.
 */
EntityAction DrawInspectorPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected,
    std::vector<std::string> const& inKnownTextures ) noexcept;

/**
 * @brief Restarts the "Entity #N" fallback numbering (see GetEntityLabel)
 *        at 0 -- call when starting a fresh scene (File > New), so a new
 *        scene's unnamed entities count up from 0 instead of continuing
 *        wherever the previous scene's counter left off.
 */
void ResetEntityDisplayIds() noexcept;
