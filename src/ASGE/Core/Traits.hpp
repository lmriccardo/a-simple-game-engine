#pragma once

#include <type_traits>
#include <vector>
#include <variant>
#include <tuple>

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

template<typename T>
struct tuple_to_variant { using type = T; };

template<typename ... Ts>
struct tuple_to_variant<std::tuple<Ts...>> { using type = std::variant<Ts...>; };

template<typename T>
using tuple_to_variant_t = tuple_to_variant<T>::type;

template<typename Tuple>
struct tuple_is_homogeneous;

template<typename T, typename... Rest>
struct tuple_is_homogeneous<std::tuple<T, Rest...>>
    : std::bool_constant<(std::is_same_v<T, Rest> && ...)>
{};

template<typename Tuple>
inline constexpr bool tuple_is_homogeneous_v = tuple_is_homogeneous<Tuple>::value;

// ---- trait: T if homogeneous, variant<Ts...> otherwise ----
template<typename Tuple, bool = tuple_is_homogeneous_v<Tuple>>
struct tuple_fold_type;

// homogeneous specialization: collapse to the single shared T
template<typename T, typename... Rest>
struct tuple_fold_type<std::tuple<T, Rest...>, true>
{
    using type = T;
};

// heterogeneous specialization: fall back to variant<Ts...>
template<typename... Ts>
struct tuple_fold_type<std::tuple<Ts...>, false>
{
    using type = std::variant<Ts...>;
};

template<typename Tuple>
using tuple_fold_type_t = typename tuple_fold_type<Tuple>::type;

}