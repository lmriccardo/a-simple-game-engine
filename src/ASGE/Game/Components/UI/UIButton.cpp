#include "UIButton.hpp"

void asge::game::scene::Serializer<asge::game::components::UIButton>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    Serializer<components::details::StateColors>::ToToml( inValue.m_Colors, table, inCtx );
}

asge::game::components::UIButton
asge::game::scene::Serializer<asge::game::components::UIButton>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UIButton result
    {
        .m_Colors = Serializer<components::details::StateColors>::FromToml(table, inCtx)
    };

    return result;
}
