#pragma once

#include <vector>
#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/Math/Geometry/Rect.hpp>

namespace asge::game::resources
{

/** @brief One clickable entity's hit-test rect for a single frame -- see UIHitList. */
struct HitEntry
{
    ecs::Entity m_Entity;      // The UIButton-carrying entity this rect belongs to
    math::Rect  m_Rect;        // World- or screen-space rect, per m_ScreenSpace, to test the pointer against
    bool        m_ScreenSpace; // Whether m_Rect is in screen space (test against raw mouse position) or world space (test against the camera-unprojected position)
};

/**
 * @brief Registry-wide UI hit-test list for the frame just drawn, back to front.
 *
 * Opt-in: only populated when this resource has been set (via
 * Registry::SetResource<UIHitList>({})), and only then does RenderSystem
 * rebuild it every frame from that frame's draw order. systems::
 * UIButtonSystem walks it back-to-front (frontmost/topmost entity wins ties)
 * to resolve which UIButton, if any, the pointer is over.
 */
struct UIHitList
{
    std::vector<HitEntry> m_Entries;
};

}