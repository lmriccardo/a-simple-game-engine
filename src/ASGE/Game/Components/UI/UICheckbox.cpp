#include "UICheckbox.hpp"

void asge::game::scene::Serializer<asge::game::components::UICheckbox>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    Serializer<components::details::StateColors>::ToToml( inValue.m_BoxColors, table, inCtx );
    table.Set( "m_CheckColor", static_cast<std::int64_t>( graphics::RGBATo32A( inValue.m_CheckColor ) ) );
    table.Set( "m_Checked", inValue.m_Checked );
}

asge::game::components::UICheckbox
asge::game::scene::Serializer<asge::game::components::UICheckbox>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UICheckbox result;

    result.m_BoxColors = Serializer<components::details::StateColors>::FromToml(table, inCtx);

    auto const dColor = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_CheckColor ) );
    result.m_CheckColor = graphics::C32AToRGBA( 
        static_cast<graphics::Color32A>( table.Get( "m_CheckColor", dColor ) ) );

    result.m_Checked = table.Get( "m_Checked", result.m_Checked );

    return result;
}