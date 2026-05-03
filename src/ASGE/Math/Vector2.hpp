#pragma once

namespace asge::math
{

struct Vector2
{
    float x; // The x coordinate value
    float y; // The y coordinate value

    Vector2(float inX, float inY) : x(inX), y(inY) {};
    Vector2(Vector2 const&) = default;
    Vector2(Vector2&&) = default;
    
    Vector2& operator=(Vector2 const&) = default;
    Vector2& operator=(Vector2&&) = default;

    static Vector2 Zero()
    { return Vector2{ 0.0f, 0.0f }; }

    Vector2 operator+(Vector2 const& inOther) const;
    Vector2 operator-(Vector2 const& inOther) const;
    Vector2 operator*(float inScalar) const;
    
    Vector2& operator+=(Vector2 const& inOther);
    Vector2& operator-=(Vector2 const& inOther);
};

inline Vector2 operator*(float inScalar, Vector2 const& inVec)
{
    return Vector2{ inVec.x * inScalar, inVec.y * inScalar };
}

}