#pragma once

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::video
{

// A screen-space rectangle draw calls are clipped/offset into (see IRenderer::SetViewport)
using Viewport = math::Rect;

/**
 * @brief A simple 2D camera: a world-space position plus a uniform zoom
 *
 * Consumed by IRenderer::SetCamera and the WorldToScreen/ScreenToWorld/
 * TransformRect helpers below to convert world-space coordinates passed to
 * draw calls into screen-space ones. No rotation -- position and zoom only.
 */
struct Camera
{
    float m_X   { 0.0f }; // world-space X the camera is centered/anchored on
    float m_Y   { 0.0f }; // world-space Y the camera is centered/anchored on
    float m_Zoom{ 1.0f }; // uniform scale applied after translating by the camera
};

/** @brief Converts a world-space point to screen space via inCamera's position and zoom. */
math::Float2 WorldToScreen(Camera const& inCamera, math::Float2 inWorldPos) noexcept;

/**
 * @brief Converts a world-space point to screen space, offsetting by inViewport's origin
 *
 * Viewport clipping is already handled by the renderer backend -- this only
 * adds inViewport's (x, y) so the camera is anchored within it.
 */
math::Float2 WorldToScreen(
    Camera const& inCamera, Viewport const& inViewport, math::Float2 inWorldPos
) noexcept;

// Inverse of the two-argument WorldToScreen: maps a screen-space point back to world space
math::Float2 ScreenToWorld(
    Camera const& inCamera, Viewport const& inViewport, math::Float2 inScreenPos
) noexcept;

// Applies WorldToScreen to inWorldRect's position and inCamera's zoom to its size
math::Rect TransformRect(Camera const& inCamera, math::Rect const& inWorldRect) noexcept;

/**
 * @brief The world-space rect currently visible through inViewport at inCamera's position/zoom.
 *
 * Inverse of TransformRect's size math: inViewport's size divided by zoom
 * gives how much world-space area is on screen, anchored at the camera's
 * own (m_X, m_Y). Used by systems::RenderSystem to cull sprites whose
 * destination rect doesn't overlap it at all, rather than drawing (and
 * having the backend clip) every entity in the world every frame.
 */
math::Rect VisibleWorldRect(Camera const& inCamera, Viewport const& inViewport) noexcept;

}
