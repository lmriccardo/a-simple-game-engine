#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Camera.hpp>
#include <ASGE/Input/InputState.hpp>

namespace asge::game::systems
{

/**
 * @brief Resolves this frame's hover/click against resources::UIHitList.
 *
 * Updates every Interactable's m_Hovered/m_Held/m_Clicked, walking the hit list
 * back to front so the topmost entity wins. A no-op without the resource, and
 * a disabled Interactable never hovers or wins a click. Also fires the sibling
 * UIButton's m_OnClick on the click edge.
 */
void UIInteractionSystem(
    ecs::Registry& inReg, input::InputState const& inInput,
    video::Camera const& inCamera);

/**
 * @brief Places and resizes each UIPanel's children into the slots of its layout.
 *
 * Children (via Hierarchy, in sibling order) fill the Grid/VStack/HStack slots,
 * inset by margin + padding with m_Spacing between. They are moved and their
 * UIRect stretched per UILayoutItem; sprites and auto-sized labels never
 * stretch. Nested panels follow their parent. Run before
 * TransformPropagationSystem.
 */
void UILayoutSystem( ecs::Registry& inReg ) noexcept;

/**
 * @brief Refreshes every UIDebugOverlay label with the FPS and entity count.
 *
 * The text reads "FPS min / avg / max | Entities: M", taking the frame time
 * from TimeSystem (unscaled, so pausing or slow motion doesn't skew the FPS).
 * FPS min/avg/max are taken over each overlay's m_RefreshInterval; the text
 * only changes once per interval. Call it once per frame from Update().
 */
void UIDebugOverlaySystem( ecs::Registry& inReg ) noexcept;

}