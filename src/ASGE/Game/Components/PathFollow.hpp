#pragma once

#include <ASGE/Core/Math/Geometry/CatmullRomSpline.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

struct PathFollow
{
    std::vector<math::Float2> m_Waypoints;
    float m_Speed{ 1.0f };
    bool m_Loop{ false };

    // Runtime-only parameters
    math::CatmullRomSpline m_Path;
    float m_Traveled{0.0f};
    bool m_Finished{false};
};

template<>
struct Serializer<PathFollow>
{
    static constexpr str::StringView kTableName = "PathFollow";

    using T = PathFollow;

    static void ToToml( T inValue, asge::config::toml::TOMLTableView inTview ) noexcept;
    static T FromToml( asge::config::toml::TOMLTableView inTview ) noexcept;
};

}