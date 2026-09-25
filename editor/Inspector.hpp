#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

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
 * @param inHasProject "Create Entity" is disabled while false -- a created
 *        entity gets no SceneId to tag it into anything without an active
 *        project/scene, so it'd just be an orphan Save can never reach.
 * @return True the one frame a "Create" click happened -- the caller (which
 *         owns the SceneManager this Registry belongs to, unlike this file)
 *         performs the actual Registry::CreateEntity() + SceneId tagging.
 */
bool DrawEntityListPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected, bool inHasProject ) noexcept;

/**
 * @brief Which entity-lifecycle action (if any) DrawInspectorPanel's buttons
 *        requested this frame. ComponentsChanged covers Add/Remove Component
 *        plus a Sprite/Animation/AudioSource path selection changing -- the
 *        caller uses it to know when to re-run AssetManager::ResolveAssets
 *        (a newly-added/repointed one has nothing resolved yet) without
 *        having to do it unconditionally every frame, which would re-log
 *        the same failure for anything that stays unresolved.
 */
enum class EntityAction { None, Delete, Duplicate, ComponentsChanged };

/**
 * @brief DrawInspectorPanel's full per-frame report: m_Action drives
 *        main.cpp's existing entity-lifecycle/ResolveAssets switch
 *        unchanged; m_FieldChanged (Phase 11) is a separate, broader
 *        "did anything on this entity change at all" signal for marking
 *        the active scene unsaved -- true for every field edit (not just
 *        the narrow set ComponentsChanged covers) plus whenever m_Action
 *        isn't None. Kept apart from m_Action deliberately: a continuously-
 *        dragged field (Position, Zoom, ...) fires every frame it's held,
 *        which is fine to mark dirty every time but not safe to re-run
 *        ResolveAssets on that often -- see Inspector.cpp's DrawInspector
 *        doc comment.
 */
struct InspectorResult
{
    EntityAction m_Action = EntityAction::None;
    bool         m_FieldChanged = false;
};

/**
 * @brief Cross-cutting state for a PathFollow's "Select Waypoints" viewport
 *        mode (Phase 12): owned and driven by main.cpp (viewport clicks,
 *        ESC-cancel, the overlay showing points as they're placed), toggled
 *        by PathFollow's own DrawInspector section, whose button flips
 *        m_Active and snapshots the pre-edit m_Waypoints into m_Snapshot so
 *        an ESC-cancel can restore them.
 */
struct WaypointEditState
{
    bool                             m_Active = false;
    asge::ecs::Entity                m_Entity = asge::ecs::Entity::Null();
    std::vector<asge::math::Float2>  m_Snapshot;
};

/**
 * @brief Draws one section per currently-serializable component type
 *        inSelected actually has, each editing the live Registry component
 *        directly (no intermediate copy) -- a fixed, explicit list of known
 *        types (Registry::HasComponent<T> per type), not a generic
 *        reflection system. A no-op if inSelected is Entity::Null().
 *
 * @param inKnownTextures Every texture virtual path the editor currently
 *        knows about (see editor/AssetBrowser.hpp's KnownTexturePaths) --
 *        both Sprite Add-Component and an existing Sprite's "Virtual Path"
 *        dropdown are restricted to these (plus "None" for the latter)
 *        instead of a hand-typed, unresolved m_VirtualPath.
 * @param inKnownAnimations Same as inKnownTextures, for Animation::m_ClipPath
 *        (see KnownAnimationPaths).
 * @param inKnownAudio Same as inKnownTextures, for
 *        AudioSource::m_VirtualClipPath (see KnownAudioPaths).
 * @param ioWaypointEdit See WaypointEditState's own doc comment.
 * @return See InspectorResult's own doc comment.
 */
InspectorResult DrawInspectorPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected,
    std::vector<std::string> const& inKnownTextures,
    std::vector<std::string> const& inKnownAnimations,
    std::vector<std::string> const& inKnownAudio,
    WaypointEditState& ioWaypointEdit ) noexcept;

/**
 * @brief Restarts the "Entity #N" fallback numbering (see GetEntityLabel)
 *        at 0 -- call when starting a fresh scene (File > New), so a new
 *        scene's unnamed entities count up from 0 instead of continuing
 *        wherever the previous scene's counter left off.
 */
void ResetEntityDisplayIds() noexcept;
