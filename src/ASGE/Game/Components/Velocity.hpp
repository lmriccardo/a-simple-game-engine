#pragma once

#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief Linear velocity — rate of change of position, in units per second.
 */
struct Velocity
{
    float m_DX{0.0f}; // change in X per second
    float m_DY{0.0f}; // change in Y per second
};

template<>
struct Serializer<Velocity>
{
    using T = Velocity;

    static void ToToml(
        Velocity inVelocity, asge::config::TOMLTableView inTview
    ) noexcept {
        inTview.Table("Velocity")
               .Set("m_DX", inVelocity.m_DX)
               .Set("m_DY", inVelocity.m_DY);
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto componentTable = inEnttView.Table("Velocity");

        Velocity result{};
        result.m_DX = componentTable.Get("m_DX", result.m_DX);
        result.m_DY = componentTable.Get("m_DY", result.m_DY);
        return result;
    }
};

}
