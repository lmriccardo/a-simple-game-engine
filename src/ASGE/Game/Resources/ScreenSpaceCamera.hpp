#pragma once

#include <ASGE/Video/Graphics/Camera.hpp>

namespace asge::game::resources
{

/**
 * @brief Optional override for the camera systems::RenderSystem uses while
 *        drawing screen-space content, in place of its own default identity
 *        (origin, 1x) reset.
 *
 * Absent by default -- a real shipped game never sets this, so its own
 * screen-space HUD stays exactly what it always was: fixed to the window,
 * independent of wherever systems::CameraSystem points IRenderer's camera
 * following a components::Camera entity. It exists for a tool (the level
 * editor) that hijacks IRenderer's camera for its own free-roam navigation
 * instead -- setting this resource every frame to that same navigation
 * camera makes screen-space content preview panning/zooming together with
 * everything else in the editor's viewport, instead of staying glued to the
 * window's own raw corner regardless of where the author is currently
 * looking.
 */
struct ScreenSpaceCamera
{
    video::Camera m_Camera;
};

}
