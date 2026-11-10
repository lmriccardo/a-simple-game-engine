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
    details::StateColors m_Colors; // Fill colors for normal, hover and pressed

    // Runtime
    signals::Signal<> m_OnClick{}; // Fired by systems::UIInteractionSystem on click
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UIButton>
{
    static constexpr str::StringView kTableName = "UIButton"; // Name of the TOML subtable (de)serialization uses

    using T = components::UIButton;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}