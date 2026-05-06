#pragma once

#include "VectorN.hpp"

namespace asge::math
{

template<_internal::Numeric T>
class Vec2 final : public VecN<2, T, Vec2<T>>
{
    template<std::size_t, _internal::Numeric, typename>
    friend class VecN;

public:
    using base = VecN<2, T, Vec2<T>>;

    template<_internal::Numeric U>
    using rebind = Vec2<U>;
    
    using typename base::value_type;
    using typename base::reference;
    using typename base::const_reference;

    using base::VecN; // Inherit all constructors

    static Vec2 Zero()
    {
        return Vec2( base::Zero() );
    }

    /* Conversion constructor (Crucial for the operators below) */
    Vec2(const base& inBase) : base(inBase) {}

    reference x() { return (*this)[0]; }
    reference y() { return (*this)[1]; }
    value_type x() const { return (*this)[0]; }
    value_type y() const { return (*this)[1]; }

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

using Float2 = Vec2<float>;
using Double2 = Vec2<double>;
using Int2 = Vec2<int>;
using Int642 = Vec2<std::int64_t>;

DEFINE_REBIND_TRAIT(Vec2)

}