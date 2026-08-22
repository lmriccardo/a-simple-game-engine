#pragma once

#include <cstddef>
#include "Constant.hpp"

namespace asge::ecs
{

/** @brief Dense, monotonically-assigned identifier for a component type. */
using ComponentTypeId = std::size_t;

/**
 * @brief Hands out the next unused ComponentTypeId.
 *
 * Not meant to be called directly — GetComponentTypeId<T>() is the only
 * intended caller, using it to lazily assign each type its own id once.
 *
 * @warning The counter is a plain, unsynchronized static: concurrent first
 *          calls for different types from multiple threads would race.
 */
inline ComponentTypeId NextComponentTypeId() noexcept
{
    static ComponentTypeId next = 0;
    return next++;
}

/**
 * @brief Returns the stable ComponentTypeId for type T.
 *
 * The id is assigned the first time this is called for T and cached in a
 * function-local static, so every later call for the same T returns that
 * same value for the rest of the program's run. Assignment order depends
 * on call order, so ids must not be persisted or assumed equal across
 * separate runs, builds, or processes.
 *
 * @tparam T Component type to identify.
 */
template <typename T>
ComponentTypeId GetComponentTypeId() noexcept
{
    static const ComponentTypeId id = NextComponentTypeId();
    return id;
}

/**
 * @brief Compile-time component storage capacity for a component type T.
 *
 * Defaults every component to kMaxEntities; specialize this trait for a
 * specific T to give its ComponentPool a smaller or larger Nc instead,
 * independent of how many entities the world can hold.
 *
 * @tparam T Component type being sized.
 */
template<typename T>
struct ComponentCapacity
{
    static constexpr std::size_t value = kMaxEntities;
};

template<typename T>
static constexpr std::size_t component_cap_v = ComponentCapacity<T>::value;

}