#pragma once

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Configuration/TOML_TableView.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include "IdContext.hpp"

namespace asge::game::scene
{

/**
 * @brief Customization point mapping a component type T to/from TOML.
 *        Specialize this per component (see Transform/Velocity/Sprite in
 *        Game/Components/) to give it ToToml/FromToml; the primary template's
 *        static_assert fires if some other component is used here before
 *        it has one. A specialization is also expected to declare
 *        `static constexpr std::string_view kTableName` naming the
 *        subtable ToToml/FromToml agree on — that's what lets a generic
 *        per-entity walker (see Components.hpp's SerializableComponents)
 *        ask "does this entity have a T?" via TOMLTableView::HasTable
 *        without hardcoding the name a second time.
 */
template<typename T>
struct Serializer
{
    /** @brief Writes inValue's fields into inTview. Shape is up to each specialization. */
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept
    {
        static_assert( false && "Not Implemented" );
    }

    /** @brief Reads a T back out of inTview, as previously written by ToToml. */
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
    static constexpr str::StringView kShapeName = "Rect";

    static void ToToml( math::Rect inShape, asge::config::toml::TOMLTableView inTview ) noexcept;
    static math::Rect FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

/** @brief Serializer for a Circle-shaped Collider — see Serializer<math::Rect>::kShapeName. */
template<>
struct Serializer<math::Circle>
{
    static constexpr str::StringView kShapeName = "Circle";

    static void ToToml( math::Circle inShape, asge::config::toml::TOMLTableView inTview ) noexcept;
    static math::Circle FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

/** @brief Serializer for an RGBA_Color, round-tripping r/g/b/a as ints under m_Red/m_Green/m_Blue/m_Alpha. */
template<>
struct Serializer<graphics::RGBA_Color>
{
    static void ToToml( graphics::RGBA_Color inColor, asge::config::toml::TOMLTableView inTview ) noexcept;
    static graphics::RGBA_Color FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

}
