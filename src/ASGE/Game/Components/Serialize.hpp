#pragma once

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Circle.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>

namespace asge::game::components
{

/**
 * @brief Customization point mapping a component type T to/from TOML.
 *        Specialize this per component (see Transform/Velocity/Sprite in
 *        this folder) to give it ToToml/FromToml; the primary template's
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
    static void ToToml( T inValue, asge::config::TOMLTableView inTview ) noexcept
    {
        static_assert( false && "Not Implemented" );
    }

    /** @brief Reads a T back out of inTview, as previously written by ToToml. */
    static T FromToml( asge::config::TOMLTableView inTview ) noexcept
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

    static void ToToml( 
        math::Rect inShape, asge::config::TOMLTableView inTview 
    ) noexcept  {
        inTview.Set("m_Width",   inShape.w)
               .Set("m_Height",  inShape.h)
               .Set("m_OffsetX", inShape.x)
               .Set("m_OffsetY", inShape.y);
    }

    static math::Rect FromToml( asge::config::TOMLTableView inTview ) noexcept
    {
        return math::Rect{
            inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f),
            inTview.Get("m_Width", 0.0f), inTview.Get("m_Height", 0.0f)
        };
    }
};

/** @brief Serializer for a Circle-shaped Collider — see Serializer<math::Rect>::kShapeName. */
template<>
struct Serializer<math::Circle>
{
    static constexpr str::StringView kShapeName = "Circle";

    static void ToToml( 
        math::Circle inShape, asge::config::TOMLTableView inTview 
    ) noexcept  {
        inTview.Set("m_OffsetX", inShape.m_Center.x())
               .Set("m_OffsetY", inShape.m_Center.y())
               .Set("m_Radius",  inShape.m_Radius);
    }

    static math::Circle FromToml( asge::config::TOMLTableView inTview ) noexcept
    {
        return math::Circle{
            math::Float2{ inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f) },
            inTview.Get("m_Radius",  0.0f)
        };
    }
};

}