#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include "Common.hpp"

namespace asge::game::components
{

/**
 * @brief The click-signal and fill-color half of a clickable UI widget.
 *
 * A full button is UIButton + Interactable (hover/held/click state) +
 * UIRect (its rectangular footprint); add UILabel for a text caption.
 * RenderSystem's Draw(UIRect) picks m_Colors' m_Color/m_HoverColor/
 * m_PressedColor by the sibling Interactable's m_Hovered/m_Held (held wins),
 * and systems::UIInteractionSystem fires m_OnClick once, the frame that same
 * Interactable transitions from held to released while still hovered.
 */
struct UIButton
{
    // Serialized
    details::StateColors m_Colors;

    // Runtime
    signals::Signal<> m_OnClick; // Fired by systems::UIInteractionSystem on click
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Colors only -- see AudioSource's Serializer doc
 *        comment for why a scene file describes what a button looks like,
 *        not its live hover/press state (that's Interactable's, and isn't
 *        serialized either). FromToml leaves m_OnClick with no subscribers,
 *        same reasoning: a Signal isn't serializable at all.
 */
template<>
struct Serializer<components::UIButton>
{
    static constexpr str::StringView kTableName = "UIButton";

    using T = components::UIButton;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}