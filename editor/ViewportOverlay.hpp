#pragma once

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Path.hpp>
#include <ASGE/Game/Components/PathFollow.hpp>

#include <optional>
#include <vector>

struct ImDrawList;

/**
 * @brief inEntity's world-space bounding rect for viewport purposes
 *        (picking, gizmo placement): its Sprite's destination rect if it has
 *        one and it's resolved, else its UIRect's own rect (Phase 16 --
 *        same RectFromSize math RenderSystem.cpp's own Collect<UIRect>
 *        uses), else a 1x1 point at inTransform's position -- the one place
 *        this fallback is defined, shared by PickEntityAt (main.cpp) and
 *        the gizmo below.
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
 *        coordinates-only editor. Skips a Registry::IsDisabled() entity --
 *        one that wouldn't actually collide at runtime shouldn't still show
 *        as if it would.
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
 * entities at all. Skips a Registry::IsDisabled() entity, same reasoning as
 * DrawColliderOverlays.
 */
void DrawCameraOverlays(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, ImDrawList* inDrawList,
    int inTargetWidth, int inTargetHeight ) noexcept;

/** @brief Maps a point local to inRoute's Transform (scale, then rotation, then translation) into world space. */
asge::math::Float2 PathLocalToWorld( asge::game::components::Transform const& inRoute, asge::math::Float2 inLocal ) noexcept;

/** @brief Inverse of PathLocalToWorld; a zero scale axis maps that axis to 0 rather than dividing by zero. */
asge::math::Float2 PathWorldToLocal( asge::game::components::Transform const& inRoute, asge::math::Float2 inWorld ) noexcept;

/**
 * @brief World-anchored (but fixed screen-pixel-sized, so they don't grow/
 *        shrink with zoom) drop/pin markers plus straight connecting lines,
 *        in placement order, for a Path's waypoints. Waypoints are stored
 *        local to the route entity's Transform and are drawn here in world
 *        space through it. Drawn for the selected entity whenever it has a
 *        Path, brighter while "Select Waypoints" mode is actively editing
 *        it. Outside that mode, also overlays the Catmull-Rom curve (sampled
 *        at Path::m_Resolution per segment) on top of the straight lines.
 *        Waypoint 0 gets a ring marking it as locked to the route origin,
 *        and inSelectedIndex (if valid) is drawn larger and white.
 *        A no-op for a Path with no waypoints.
 */
void DrawPathWaypointOverlay(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList,
    asge::game::components::Transform const& inRoute, asge::game::components::Path const& inPath,
    bool inActive, int inSelectedIndex = -1 ) noexcept;

/** @brief Where a PathFollow sits on its Path: local position (offsets included) and, if aligned and defined, local rotation. */
struct PathPose
{
    asge::math::Float2   m_Position;
    std::optional<float> m_Rotation; // path tangent angle in radians; nullopt when m_AlignRotation is off or the tangent is degenerate
    std::optional<asge::math::Float2> m_Heading; // unit path tangent regardless of m_AlignRotation; nullopt when the tangent is degenerate
};

/**
 * @brief The pose a follower with inFollow's offsets/align flag has at
 *        inDistance along inPath, in the route's local space -- the same
 *        values PathFollowingSystem writes. Builds a temporary spline when
 *        m_Path isn't built yet; nullopt if the Path has fewer than 2 points.
 */
std::optional<PathPose> SamplePathPose(
    asge::game::components::Path const& inPath, asge::game::components::PathFollow const& inFollow,
    float inDistance ) noexcept;

/**
 * @brief Draws a translucent copy of every child of inRoute that has a
 *        PathFollow, Transform and a resolved Sprite, placed on inRoute's
 *        Path at its start and at inProgress (0..1) of the path length, so
 *        the sprite's real extent against the line can be checked.
 *        inProgress == 0 draws only the start ghost.
 */
void DrawPathFollowerGhosts(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inRoute, float inProgress ) noexcept;

/**
 * @brief Draws an arrow showing the path heading of a PathFollow entity.
 *
 * Anchored at the follower's place on its parent's Path (at its current
 * PathFollow::m_Traveled) and pointing along the path tangent, carried
 * through the parent's transform. A fixed screen-space length, so it reads
 * the same at any zoom. A no-op if inFollower has no PathFollow or its
 * parent has no Path, or if the tangent is degenerate at that point.
 */
void DrawPathFollowerHeading(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inFollower ) noexcept;

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
