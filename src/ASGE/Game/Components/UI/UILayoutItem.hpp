#pragma once

#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Where a child sits along one axis of a layout slot it doesn't fill.
 *
 * Only used on an axis whose UILayoutItem fill flag is off.
 */
enum class SlotAlign
{
    Start,  // Against the slot's left/top edge
    Center, // Centered in the slot
    End     // Against the slot's right/bottom edge
};

/**
 * @brief Per-child layout options.
 *
 * Read by systems::UILayoutSystem when the entity's parent is a UIPanel.
 *
 * Without this component a child stretches to fill its slot on both axes and
 * sits at the slot's start. With it, each axis is independent: m_FillX/m_FillY
 * stretch the child's UIRect to the slot, and an axis that doesn't fill keeps
 * the child's own size and is placed by m_AlignX/m_AlignY.
 */
struct UILayoutItem
{
    // Serialized fields
    bool        m_FillX  { true };               // Stretch the width to the slot
    bool        m_FillY  { true };               // Stretch the height to the slot
    SlotAlign   m_AlignX { SlotAlign::Start };   // Horizontal placement when not filling X
    SlotAlign   m_AlignY { SlotAlign::Start };   // Vertical placement when not filling Y
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UILayoutItem>
{
    static constexpr str::StringView kTableName = "UILayoutItem"; // Name of the TOML subtable (de)serialization uses
    using T = components::UILayoutItem;

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
