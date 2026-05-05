#pragma once

#include "Vectors.hpp"

namespace asge::math
{

template<_internal::Numeric T>
class Vec2 final : public VectorN<2, T, Vec2<T>>
{
    template<std::size_t, _internal::Numeric, typename>
    friend class VectorN;

public:
    using base = VectorN<2, T, Vec2<T>>;
    
    using typename base::value_type;
    using typename base::reference;
    using typename base::const_reference;

    using base::VectorN; // Inherit all constructors

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

    Vec2 operator+(const Vec2& other) const noexcept
    {
        return Vec2(base::operator+(other));
    }
    Vec2 operator-(const Vec2& other) const noexcept
    {
        return Vec2(base::operator-(other));
    }
    Vec2 operator*(const Vec2& other) const noexcept
    {
        return Vec2(base::operator*(other));
    }
    Vec2 operator/(const Vec2& other) const noexcept
    {
        return Vec2(base::operator/(other));
    }

    template<_internal::Numeric U>
    auto operator*(U scalar) const noexcept 
    { 
        return Vec2(base::operator*(scalar));
    }

    using base::operator+=;
    using base::operator-=;
    using base::operator*=;
    using base::operator/=;

private:
    using base::operator[];
};

using Float2 = Vec2<float>;

}