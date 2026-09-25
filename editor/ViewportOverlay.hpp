#pragma once

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Transform.hpp>

#include <vector>

struct ImDrawList;

/**
 * @brief inEntity's world-space bounding rect for viewport purposes
 *        (picking, gizmo placement): its Sprite's destination rect if it
 *        has one and it's resolved, else a 1x1 point at inTransform's
 *        position -- the one place this fallback is defined, shared by
 *        PickEntityAt (main.cpp) and the gizmo below.
 */
asge::math::Rect GetEntityWorldBounds(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity,
    asge::game::components::Transform const& inTransform ) noexcept;

/**
 * @brief World-space coordinate grid overlay, panning/zooming with the
 *        camera so lines line up with the actual scene -- unlike the
 *        inspector's numbers, this makes a position readable directly off
 *        the viewport.
 * @param inSpacing World units between grid lines; editor-configurable.
 */
void DrawWorldGrid( asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, float inSpacing ) noexcept;

/**
 * @brief Translucent Collider bounds overlay (Rect/Circle), colored by
 *        Collider::m_Layer -- the actual visual value-add over a
 *        coordinates-only editor.
 */
void DrawColliderOverlays(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, ImDrawList* inDrawList ) noexcept;

/**
 * @brief Letterboxed preview of what a game window of inTargetWidth x
 *        inTargetHeight pixels would actually show, top-left corner
 *        anchored to world origin (flush with the world-origin axis lines
 *        DrawWorldGrid draws) -- drawn as a world-space rect, so it pans/
 *        zooms with the editor's own camera exactly like the grid/colliders
 *        do, not a fixed screen-space frame that ignores zoom. Everything
 *        in the editor's current viewport outside that rect is dimmed like
 *        a real letterbox; if the rect doesn't overlap the viewport at all
 *        (panned/zoomed away from it), the whole viewport dims. This is the
 *        one, single "what the shipped game's window shows" preview --
 *        DrawCameraOverlays below is the separate, per-entity one. A no-op
 *        if either dimension is non-positive.
 */
void DrawGameWindowPreview(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList,
    int inTargetWidth, int inTargetHeight ) noexcept;

/**
 * @brief One preview box per entity carrying a Camera (+ Transform) --
 *        same shape as DrawColliderOverlays: every match gets its own box,
 *        drawn as a world-space rect (so it pans/zooms with the editor's
 *        own camera, same as everything else here) rather than a fixed
 *        screen-space frame.
 *
 * Mirrors systems::CameraSystem's own centering math (RenderSystem.cpp):
 * were this entity the active camera in a inTargetWidth x inTargetHeight
 * game window, its Transform would end up centered in it at its own
 * Camera::m_Zoom -- the box is exactly that. Purely read-only: unlike
 * CameraSystem, this never calls IRenderer::SetCamera, so it can never
 * move the editor's own pan/zoom no matter how many Camera entities exist
 * or which (if any) is resources::ActiveCamera. A no-op if either
 * dimension is non-positive, or draws nothing for a scene with no Camera
 * entities at all.
 */
void DrawCameraOverlays(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, ImDrawList* inDrawList,
    int inTargetWidth, int inTargetHeight ) noexcept;

/**
 * @brief World-space dots (plus connecting lines, in placement order) for a
 *        PathFollow's waypoints -- drawn while Phase 12's "Select Waypoints"
 *        mode is active for it, so points are visible as they're clicked in.
 *        A no-op for fewer than one waypoint.
 */
void DrawPathFollowWaypointOverlay(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList,
    std::vector<asge::math::Float2> const& inWaypoints ) noexcept;

/** @brief Which translate-gizmo handle (if any) a hit-test landed on -- see HitTestGizmo. */
enum class GizmoAxis { None, X, Y };

/**
 * @brief Draws the selected entity's translate gizmo: four arrows (E/W for
 *        X, N/S for Y), each anchored at its own edge's midpoint on
 *        GetEntityWorldBounds and pointing outward -- a no-op if inSelected
 *        has no Transform. Fixed screen-space arm length regardless of
 *        zoom, so handles stay equally grabbable at any zoom level. Each
 *        pair is functionally one axis (dragging either arrow moves both
 *        directions along it); the second arrow per axis is purely so a
 *        handle sits next to every side of the entity, not further reach.
 */
void DrawTranslateGizmo(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inSelected, ImDrawList* inDrawList ) noexcept;

/**
 * @brief Which of inSelected's gizmo arms (if any) inScreenPos falls on --
 *        checked before general viewport picking so handles take priority
 *        over picking a different/the same entity underneath them.
 */
GizmoAxis HitTestGizmo(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inSelected, asge::math::Float2 inScreenPos ) noexcept;
