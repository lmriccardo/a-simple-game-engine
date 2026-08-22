#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include "Traits.hpp"

namespace asge::_internal::traits
{

template <typename Tuple, std::size_t... Is>
constexpr auto has_nullptr_impl(const Tuple& t, std::index_sequence<Is...>)
{
    return ( (std::get<Is>(t) == nullptr) || ... );
}

/**
 * @brief Checks whether any element of a tuple of pointers is null.
 *
 * @tparam Ts Element types; each must be a pointer or std::nullptr_t.
 * @param t Tuple to inspect.
 * @return true if at least one element compares equal to nullptr.
 */
template<typename... Ts>
constexpr bool has_nullptr( const std::tuple<Ts...>& t )
    requires ( (std::is_pointer_v<Ts> || std::is_null_pointer_v<Ts>) && ... )
{
    return has_nullptr_impl( t, std::make_index_sequence<sizeof...(Ts)>{} );
}

/**
 * @brief Fetches tuple element Icurr, already wrapped in its fold type.
 *
 * Built via std::in_place_index for heterogeneous tuples, so the element
 * lands in its own variant alternative without an ambiguous converting
 * constructor call when the tuple repeats a type.
 */
template<std::size_t Icurr, typename Tuple>
auto get_element_as_fold_type( Tuple const& t )
{
    if constexpr ( tuple_is_homogeneous_v<Tuple> ) return std::get<Icurr>(t);
    else {
        return tuple_fold_type_t<Tuple>{ std::in_place_index<Icurr>, std::get<Icurr>(t) };
    }
}

}
