#pragma once

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Transform.hpp>

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
