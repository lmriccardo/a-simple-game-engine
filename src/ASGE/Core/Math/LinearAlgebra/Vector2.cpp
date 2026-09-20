#include "Vector2.hpp"

asge::math::Float2 asge::math::Rotate(Float2 inVector, float inAngle) noexcept
{
    float const c = std::cos(inAngle);
    float const s = std::sin(inAngle);
    return { inVector.x() * c - inVector.y() * s, inVector.x() * s + inVector.y() * c };
}