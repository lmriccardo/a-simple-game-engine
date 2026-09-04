#pragma once

#include <ASGE/Core/Strings.hpp>
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

    /** @brief The subtable name ToToml/FromToml agree on — see Serializer<Transform>::kTableName. */
    static constexpr str::StringView kTableName = "Velocity";

    static void ToToml(
        Velocity inVelocity, asge::config::toml::TOMLTableView inTview
    ) noexcept {
        inTview.Table(std::string(kTableName))
               .Set("m_DX", inVelocity.m_DX)
               .Set("m_DY", inVelocity.m_DY);
    }

    static T FromToml( asge::config::toml::TOMLTableView inEnttView ) noexcept
    {
        auto componentTable = inEnttView.Table(std::string(kTableName));

        Velocity result{};
        result.m_DX = componentTable.Get("m_DX", result.m_DX);
        result.m_DY = componentTable.Get("m_DY", result.m_DY);
        return result;
    }
};

}
