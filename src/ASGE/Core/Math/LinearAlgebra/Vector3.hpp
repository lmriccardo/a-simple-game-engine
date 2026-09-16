#pragma once

#include "VectorN.hpp"

namespace asge::math
{

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

    static Vec3 Zero()
    {
        return Vec3( base::Zero() );
    }

    Vec3() = default;

    template<_internal::Numeric... Us>
    requires (sizeof...(Us) == 3)
    explicit Vec3( Us... inValues ) : base( inValues... )
    {}

    Vec3( std::initializer_list<T> inList ) : base( inList )
    {}

    Vec3(const base& inBase) : base(inBase) {}

    reference x() { return (*this)[0]; }
    reference y() { return (*this)[1]; }
    reference z() { return (*this)[2]; }
    value_type x() const { return (*this)[0]; }
    value_type y() const { return (*this)[1]; }
    value_type z() const { return (*this)[2]; }

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