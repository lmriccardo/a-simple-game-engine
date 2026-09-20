#pragma once

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

struct ImDrawList;

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
