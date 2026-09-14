#pragma once

#include "Serialize.hpp"

namespace asge::game::components
{

struct Camera
{
    float m_Zoom        { 1.0f };
    float m_Smoothing   { 0.0f };
};

template<>
struct Serializer<Camera>
{
    static constexpr str::StringView kTableName = "Camera";

    using T = Camera;

    static void ToToml( T inValue, asge::config::toml::TOMLTableView inTview ) noexcept;
    static T FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

}