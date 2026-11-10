#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include "Common.hpp"

namespace asge::game::components
{

/**
 * @brief A toggleable box, drawn and hit-tested like UIButton.
 *
 * A full checkbox is UICheckbox + Interactable (hover/held/click state) +
 * UIRect (its rectangular footprint); add UILabel for a caption alongside
 * it. RenderSystem's Draw(UIRect) picks m_BoxColors' m_Color/m_HoverColor/
 * m_PressedColor by the sibling Interactable's m_Hovered/m_Held (held wins,
 * same as UIButton), then draws an inset m_CheckColor square on top when
 * m_Checked is true. systems::UIInteractionSystem flips m_Checked and fires
 * m_OnToggled once, the frame that same Interactable transitions from held
 * to released while still hovered.
 */
struct UICheckbox
{
    // Serialized fields
    details::StateColors m_BoxColors;                                  // Box fill colors for normal, hover and pressed
    graphics::RGBA_Color m_CheckColor   { graphics::colors::s_Black }; // Color of the inset check square
    bool                 m_Checked      { false };                     // Whether the box is currently checked

    // Runtime attributes
    signals::Signal<bool> m_OnToggled; // Fires when new check state
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UICheckbox>
{
    static constexpr str::StringView kTableName = "UICheckbox"; // Name of the TOML subtable (de)serialization uses
    using T = components::UICheckbox;

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