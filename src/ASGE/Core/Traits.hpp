#pragma once

#include <type_traits>
#include <vector>
#include <variant>

namespace asge::_internal::traits
{

template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

template<typename T>
constexpr bool is_vector_v = is_vector<T>::value;

template<typename T, typename Variant>
struct variant_contains;

template<typename T, typename ...Ts>
struct variant_contains<T, std::variant<Ts...>>
    : std::disjunction<std::is_same<T, Ts>...> 
{};

template<typename T, typename Variant>
constexpr bool variant_contains_v = variant_contains<T, Variant>::value;

template<typename>
inline constexpr bool always_false_v = false;

}