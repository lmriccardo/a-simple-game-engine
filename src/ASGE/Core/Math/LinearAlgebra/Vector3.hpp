#pragma once

#include "VectorN.hpp"

namespace asge::math
{

/**
 * @brief A three-component vector of numeric type T, with named x(), y() and z() accessors.
 *
 * Builds on VecN<3, T>, inheriting its arithmetic operators and storage. The
 * Float3, Double3, Int3 and Int643 aliases cover the common types.
 */
template<_internal::Numeric T>
class Vec3 final : public VecN<3, T, Vec3<T>>
{
    template<std::size_t, _internal::Numeric, typename>
    friend class VecN;

public:
    using base = VecN<3, T, Vec3<T>>;

    template<_internal::Numeric U>
    using rebind = Vec3<U>;
    
    using typename base::value_type;
    using typename base::reference;
    using typename base::const_reference;

    /**
     * @brief Returns the vector (0, 0, 0).
     */
    static Vec3 Zero()
    {
        return Vec3( base::Zero() );
    }

    /**
     * @brief Leaves the components uninitialized.
     */
    Vec3() = default;

    /**
     * @brief Builds the vector from exactly three numbers (x, y, z), converted to T.
     */
    template<_internal::Numeric... Us>
    requires (sizeof...(Us) == 3)
    constexpr explicit Vec3( Us... inValues ) : base( inValues... )
    {}

    /**
     * @brief Builds the vector from a brace list; missing components are 0.
     *
     * Extra values past the third are ignored.
     */
    constexpr Vec3( std::initializer_list<T> inList ) : base( inList )
    {}

    /**
     * @brief Converts from the underlying VecN, e.g. the result of an operator.
     */
    Vec3(const base& inBase) : base(inBase) {}

    /**
     * @brief The first component, by reference.
     */
    constexpr reference x() { return (*this)[0]; }

    /**
     * @brief The second component, by reference.
     */
    constexpr reference y() { return (*this)[1]; }

    /**
     * @brief The third component, by reference.
     */
    constexpr reference z() { return (*this)[2]; }

    /**
     * @brief The first component, by value.
     */
    constexpr value_type x() const { return (*this)[0]; }

    /**
     * @brief The second component, by value.
     */
    constexpr value_type y() const { return (*this)[1]; }

    /**
     * @brief The third component, by value.
     */
    constexpr value_type z() const { return (*this)[2]; }

    using base::operator+;
    using base::operator-;
    using base::operator*;
    using base::operator/;
    using base::operator+=;
    using base::operator-=;
    using base::operator*=;
    using base::operator/=;

private:
    using base::operator[];
};

using Float3 = Vec3<float>;
using Double3 = Vec3<double>;
using Int3 = Vec3<int>;
using Int643 = Vec3<std::int64_t>;

DEFINE_REBIND_TRAIT(Vec3)

}