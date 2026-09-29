#include "UILayoutItem.hpp"

namespace
{

using asge::game::components::SlotAlign;

asge::str::String ToString( SlotAlign inValue ) noexcept
{
    switch ( inValue )
    {
    case SlotAlign::Start:  return "Start";
    case SlotAlign::Center: return "Center";
    case SlotAlign::End:    return "End";
    }

    return "Start";
}

SlotAlign FromString( asge::str::String const& inValue ) noexcept
{
    if ( inValue == "Center" ) return SlotAlign::Center;
    if ( inValue == "End" ) return SlotAlign::End;
    return SlotAlign::Start;
}

}

void asge::game::scene::Serializer<asge::game::components::UILayoutItem>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    table.Set( "m_FillX", inValue.m_FillX )
         .Set( "m_FillY", inValue.m_FillY )
         .Set( "m_AlignX", ToString( inValue.m_AlignX ) )
         .Set( "m_AlignY", ToString( inValue.m_AlignY ) );
}

asge::game::components::UILayoutItem
asge::game::scene::Serializer<asge::game::components::UILayoutItem>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UILayoutItem result;

    result.m_FillX = table.Get( "m_FillX", result.m_FillX );
    result.m_FillY = table.Get( "m_FillY", result.m_FillY );
    result.m_AlignX = FromString( table.Get( "m_AlignX", ToString( result.m_AlignX ) ) );
    result.m_AlignY = FromString( table.Get( "m_AlignY", ToString( result.m_AlignY ) ) );

    return result;
}
