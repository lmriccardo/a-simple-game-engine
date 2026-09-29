#pragma once

#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/** @brief Where a child sits along one axis of a layout slot it doesn't fill. */
enum class SlotAlign
{
    Start,  // Against the slot's left/top edge
    Center, // Centered in the slot
    End     // Against the slot's right/bottom edge
};

/**
 * @brief Per-child layout options, read by systems::UILayoutSystem when the
 *        entity's parent is a UIPanel.
 *
 * Without this component a child stretches to fill its slot on both axes and
 * sits at the slot's start. With it, each axis is independent: m_FillX/m_FillY
 * stretch the child's UIRect to the slot, and an axis that doesn't fill keeps
 * the child's own size and is placed by m_AlignX/m_AlignY.
 */
struct UILayoutItem
{
    // Serialized fields
    bool        m_FillX  { true };
    bool        m_FillY  { true };
    SlotAlign   m_AlignX { SlotAlign::Start };
    SlotAlign   m_AlignY { SlotAlign::Start };
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UILayoutItem>
{
    static constexpr str::StringView kTableName = "UILayoutItem";
    using T = components::UILayoutItem;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
