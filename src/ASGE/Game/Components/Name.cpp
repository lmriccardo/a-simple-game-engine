#include "Name.hpp"

void asge::game::scene::Serializer<asge::game::components::Name>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(str::String(kTableName)).Set( "m_Name", inValue.m_Name );
}

asge::game::components::Name asge::game::scene::Serializer<asge::game::components::Name>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(str::String(kTableName));
    T result;
    result.m_Name = table.Get<str::String>( "m_Name", result.m_Name );
    return result;
}