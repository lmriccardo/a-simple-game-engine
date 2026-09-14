#pragma once

#include <type_traits>
#include <utility>

namespace asge::ecs
{

/** @brief Type-erased base so Registry can hold every ResourceHolder<T> in one vector. */
class IResourceHolder
{
public:
    virtual ~IResourceHolder() = default;
};

/**
 * @brief Owns Registry's single instance of resource type T.
 *
 * Purely a typed box IResourceHolder can be stored/erased through — Registry
 * is the only intended caller (via SetResource<T>/GetResource<T>), which
 * `static_cast`s back to ResourceHolder<T> using the id it already tracked.
 */
template<typename T>
class ResourceHolder final : public IResourceHolder
{
public:
    T m_Value;

    explicit ResourceHolder( T inValue ) noexcept( std::is_nothrow_move_constructible_v<T> )
        : m_Value ( std::move(inValue) )
    {}
};

}
