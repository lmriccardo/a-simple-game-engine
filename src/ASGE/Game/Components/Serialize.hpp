#pragma once

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

}