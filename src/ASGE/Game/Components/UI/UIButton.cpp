#include "UIButton.hpp"

void asge::game::scene::Serializer<asge::game::components::UIButton>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set( "m_FontVirtualPath", inValue.m_FontVirtualPath )
           .Set( "m_FontWeight", inValue.m_FontWeight )
           .Set( "m_Text", inValue.m_Text )
           .Set( "m_TextAlign", str::ToString( inValue.m_TextAlignment ) )
           .Set( "m_SizeX", inValue.m_Size.x() )
           .Set( "m_SizeY", inValue.m_Size.y() );
}

asge::game::components::UIButton
asge::game::scene::Serializer<asge::game::components::UIButton>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UIButton result;
    result.m_FontVirtualPath = table.Get( "m_FontVirtualPath", str::String{} );
    result.m_FontWeight = table.Get( "m_FontWeight", result.m_FontWeight );
    result.m_Text = table.Get( "m_Text", result.m_Text );

    auto defaultAlign = str::ToString( str::TextAlign::None );
    result.m_TextAlignment = str::FromString( table.Get( "m_TextAlign", defaultAlign ) );
    
    result.m_Size = math::Float2{
        table.Get( "m_SizeX", result.m_Size.x() ),
        table.Get( "m_SizeY", result.m_Size.y() )
    };

    return result;
}

asge::math::Rect asge::game::components::CreateButtonBounds(
    Transform const &inWorld, UIButton const &inButton) noexcept
{
    return math::Rect
    {
        inWorld.m_WorldCoordinates.x(),
        inWorld.m_WorldCoordinates.y(),
        
    };
}
