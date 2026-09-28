#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Game/Components/Collider.hpp>

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
 * @brief Phase 13: which hierarchy-editing action (if any) a right-click on
 *        an entity row, or a drag-and-drop between two rows, requested this
 *        frame. Reparent is what a drop produces -- m_Target is the entity
 *        that was dragged, m_NewParent the row it was dropped onto; the
 *        other three come from the row's context menu, where m_Target is
 *        whichever entity was right-clicked.
 */
enum class HierarchyAction { None, NewChild, Detach, Remove, Reparent };

/**
 * @brief DrawEntityListPanel's full per-frame report. m_CreateClicked is
 *        the pre-Phase-13 "Create Entity" button signal, unchanged; m_Action
 *        (Phase 13) is mutually exclusive with it in practice (one user
 *        gesture per frame) but reported independently since they come from
 *        different widgets.
 */
struct EntityListResult
{
    bool              m_CreateClicked = false;
    HierarchyAction   m_Action        = HierarchyAction::None;
    asge::ecs::Entity m_Target        = asge::ecs::Entity::Null(); // entity m_Action applies to (the dragged entity, for Reparent)
    asge::ecs::Entity m_NewParent     = asge::ecs::Entity::Null(); // Reparent only: the row it was dropped onto
};

/**
 * @brief Lists every entity in inRegistry as a Hierarchy-aware tree (Phase
 *        13): a root entity (no Hierarchy, or one with no parent) per
 *        top-level node, its children nested underneath via
 *        ecs::components::ForEachChild. Clicking a row selects it;
 *        right-clicking opens New Child/Detach/Remove; dragging one row onto
 *        another reparents it there (see HierarchyAction's own doc comment).
 * @param inHasProject "Create Entity" is disabled while false -- a created
 *        entity gets no SceneId to tag it into anything without an active
 *        project/scene, so it'd just be an orphan Save can never reach.
 * @return See EntityListResult's own doc comment. The caller (which owns the
 *         SceneManager this Registry belongs to, unlike this file) performs
 *         the actual Registry::CreateEntity()/AttachChild/DetachChild/
 *         DestroyEntityGraph calls -- this file only reports what was asked.
 */
EntityListResult DrawEntityListPanel(
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
 * @brief Cross-cutting state for a Collider's "Draw Collider" viewport mode
 *        (Phase 12 step 4): owned and driven by main.cpp (drag-to-define
 *        the shape, ESC-cancel; the shape being drawn live-updates the
 *        actual Collider component, so the existing DrawColliderOverlays
 *        already shows it with no separate preview overlay needed), toggled
 *        by Collider's own DrawInspector section, whose button snapshots
 *        the pre-draw m_LocalBounds into m_Snapshot so an ESC-cancel can
 *        restore it. One-shot: a completed drag (mouse-up) exits the mode
 *        automatically rather than staying open for more edits.
 */
struct ColliderDrawState
{
    bool                                    m_Active = false;
    asge::ecs::Entity                       m_Entity = asge::ecs::Entity::Null();
    asge::game::components::ColliderShape   m_Snapshot{};
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
 * @param ioColliderDraw See ColliderDrawState's own doc comment.
 * @return See InspectorResult's own doc comment.
 */
InspectorResult DrawInspectorPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected,
    std::vector<std::string> const& inKnownTextures,
    std::vector<std::string> const& inKnownAnimations,
    std::vector<std::string> const& inKnownAudio,
    WaypointEditState& ioWaypointEdit,
    ColliderDrawState& ioColliderDraw ) noexcept;

/**
 * @brief Restarts the "Entity #N" fallback numbering (see GetEntityLabel)
 *        at 0 -- call when starting a fresh scene (File > New), so a new
 *        scene's unnamed entities count up from 0 instead of continuing
 *        wherever the previous scene's counter left off.
 */
void ResetEntityDisplayIds() noexcept;

/**
 * @brief inEntity's Name::m_Name if it has one and it's non-empty, else
 *        "Entity #N" (a stable, ever-increasing id assigned the first time
 *        this entity is seen -- see ResetEntityDisplayIds). Used by the
 *        Entities tree and Inspector header; exposed here (Phase 14) so
 *        AssetBrowser.cpp's "Attach To" submenu lists entities under the
 *        same names.
 */
std::string GetEntityLabel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept;
