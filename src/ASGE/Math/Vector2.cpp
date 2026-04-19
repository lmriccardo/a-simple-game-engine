#include "Vector2.hpp"

using namespace asge::math;

Vector2 asge::math::Vector2::operator+(Vector2 const &inOther) const
{
    return Vector2{ x + inOther.x, y + inOther.y };
}

Vector2 asge::math::Vector2::operator-(Vector2 const &inOther) const
{
    return Vector2{ x - inOther.x, y - inOther.y };
}

Vector2 asge::math::Vector2::operator*(float inScalar) const
{
    return Vector2{ x * inScalar, y * inScalar };
}

Vector2& asge::math::Vector2::operator+=(Vector2 const &inOther)
{
    x += inOther.x;
    y += inOther.y;
    return *this;
}

Vector2 &asge::math::Vector2::operator-=(Vector2 const &inOther)
{
    x -= inOther.x;
    y -= inOther.y;
    return *this;
}
