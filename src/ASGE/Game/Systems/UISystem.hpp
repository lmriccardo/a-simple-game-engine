#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Camera.hpp>
#include <ASGE/Input/InputState.hpp>

namespace asge::game::systems
{

/**
 * @brief Resolves this frame's hover/click against resources::UIHitList, 
 *        and updates every Interactable's m_Hovered/m_Held/m_Clicked from it.
 *
 * No-ops if the UIHitList resource hasn't been set (see its own doc
 * comment) -- so this is safe to call unconditionally even in states that
 * don't use UI. Walks the hit list back to front so a topmost/overlapping
 * entity wins. A disabled Interactable (m_Enabled false) is forced
 * hovered/held false and never wins a click, regardless of what
 * CollectHitList did or didn't put in the hit list for it. Also fires the
 * sibling UIButton's m_OnClick, if any, on the same held-to-released-while-
 * hovered edge that sets m_Clicked.
 */
void UIInteractionSystem(
    ecs::Registry& inReg, input::InputState const& inInput,
    video::Camera const& inCamera);

}