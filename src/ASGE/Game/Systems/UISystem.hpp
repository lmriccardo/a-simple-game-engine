#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Camera.hpp>
#include <ASGE/Input/InputState.hpp>

namespace asge::game::systems
{

/**
 * @brief Resolves this frame's hover/click against resources::UIHitList, and updates every UIButton's m_Hovered/m_Held from it.
 *
 * No-ops if the UIHitList resource hasn't been set (see its own doc
 * comment) -- so this is safe to call unconditionally even in states that
 * don't use UI. Walks the hit list back to front so a topmost/overlapping
 * entity wins; fires m_OnClick on whichever UIButton was held and released
 * while still hovered.
 */
void UIButtonSystem(
    ecs::Registry& inReg, input::InputState const& inInput,
    video::Camera const& inCamera);

}