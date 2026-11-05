#pragma once

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Formats/TOML/TOML_TableView.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/ECS/Markers.hpp>
#include "IdContext.hpp"

namespace asge::game::scene
{

/**
 * @brief Customization point mapping a component type T to/from TOML.
 *
 * Specialize this per component (see Transform/Velocity/Sprite in
 * Game/Components/) to give it ToToml/FromToml; the primary template's
 * static_assert fires if some other component is used here before it has one.
 * A specialization is also expected to declare `static constexpr
 * std::string_view kTableName` naming the subtable ToToml/FromToml agree on
 * — that's what lets a generic per-entity walker (see Components.hpp's
 * SerializableComponents) ask "does this entity have a T?" via
 * TOMLTableView::HasTable without hardcoding the name a second time.
 */
template<typename T>
struct Serializer
{
    /**
     * @brief Writes inValue's fields into inTview.
     *
     * Shape is up to each specialization.
     */
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept
    {
        static_assert( false && "Not Implemented" );
    }

    /**
     * @brief Reads a T back out of inTview.
     *
     * Expects the layout previously written by ToToml.
     */
    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept
    {
        static_assert( false && "Not Implemented" );
    }
};

/**
 * @brief Serializer for a Rect-shaped Collider.
 *
 * kShapeName is what Serializer<Collider> writes/reads as the "m_Shape"
 * discriminator, so FromToml knows which of Rect/Circle to parse the rest
 * of the table as.
 */
template<>
struct Serializer<math::Rect>
{
    static constexpr str::StringView kShapeName = "Rect"; // Value of the "m_Shape" discriminator for a Rect

    /**
     * @brief Writes the rect's offset and size into inTview.
     *
     * Keys: m_OffsetX, m_OffsetY, m_Width and m_Height.
     */
    static void ToToml( math::Rect inShape, asge::config::toml::TOMLTableView inTview ) noexcept;

    /**
     * @brief Reads a Rect back from inTview.
     *
     * Missing keys read as 0.
     */
    static math::Rect FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

/**
 * @brief Serializer for a Circle-shaped Collider.
 *
 * See Serializer<math::Rect>::kShapeName.
 */
template<>
struct Serializer<math::Circle>
{
    static constexpr str::StringView kShapeName = "Circle"; // Value of the "m_Shape" discriminator for a Circle

    /**
     * @brief Writes the circle's center and radius into inTview.
     *
     * Keys: m_OffsetX, m_OffsetY and m_Radius.
     */
    static void ToToml( math::Circle inShape, asge::config::toml::TOMLTableView inTview ) noexcept;

    /**
     * @brief Reads a Circle back from inTview.
     *
     * Missing keys read as 0.
     */
    static math::Circle FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

/**
 * @brief Serializer for the Disable marker, which has no fields.
 *
 * Only the presence of the (empty) table matters.
 */
template<>
struct Serializer<ecs::markers::Disable>
{
    static constexpr str::StringView kTableName = "Disabled"; // Name of the TOML subtable (de)serialization uses
    using T = ecs::markers::Disable;

    /**
     * @brief Writes the (empty) marker table.
     */
    static void ToToml(
        [[maybe_unused]]    T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the marker; there is nothing to read.
     */
    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
