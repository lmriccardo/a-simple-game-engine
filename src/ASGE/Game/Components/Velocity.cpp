#include "Velocity.hpp"

void asge::game::components::Serializer<asge::game::components::Velocity>::ToToml(
    Velocity inVelocity, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set("m_DX", inVelocity.m_DX)
           .Set("m_DY", inVelocity.m_DY);
}

asge::game::components::Velocity asge::game::components::Serializer<asge::game::components::Velocity>::FromToml(
    asge::config::toml::TOMLTableView inEnttView ) noexcept
{
    auto componentTable = inEnttView.Table(std::string(kTableName));

    Velocity result{};
    result.m_DX = componentTable.Get("m_DX", result.m_DX);
    result.m_DY = componentTable.Get("m_DY", result.m_DY);
    return result;
}
