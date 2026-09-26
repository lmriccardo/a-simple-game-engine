#include "UI.hpp"

#include <ASGE/Game/Components/Name.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/RenderInfo.hpp>

namespace
{

using namespace asge;
using namespace asge::game;
using namespace asge::game::ui;

Result<ecs::Entity> CreateUIEntity(
    ecs::Registry& inReg, str::String const& inName, math::Float2 inPosition,
    math::Float2 inSize, bool inScreenSpace) 
{
    auto entityR = inReg.CreateEntity();
    if ( !entityR ) return Result<ecs::Entity>::Err( entityR.Error() );
    ecs::Entity const e = entityR.Value();

    // Callback function to call when there is a failure while adding components
    // to the previously created entity for that UI Widget.
    auto const fail = [&]( auto const& inError )
    {
        auto _ = inReg.DestroyEntity( e );
        return Result<ecs::Entity>::Err( inError );
    };

    if ( !inName.empty() )
    {
        if ( auto r = inReg.AddComponent<components::Name>(e, components::Name{ inName }); !r )
        {
            return fail( r.Error() );
        }
    }

    if ( auto r = inReg.AddComponent<components::Transform>( e, components::Transform{
        .m_LocalCoordinates = inPosition, .m_Dirty = true} ); !r ) 
    {
        return fail( r.Error() );
    }

    if ( auto r = inReg.AddComponent<components::RenderInfo>( e, components::RenderInfo{
             .m_ScreenSpace = inScreenSpace } ); !r )
    {
        return fail( r.Error() );
    }

    if ( auto r = inReg.AddComponent<components::UIRect>( e, components::UIRect{ .m_Size = inSize } ); !r )
    {
        return fail( r.Error() );
    }

    return Result<ecs::Entity>::Ok( e );
}

Result<ecs::Entity> AddText( ecs::Registry& inReg, ecs::Entity inE, TextDesc const& inText, bool inAutoSize )
{
    if ( inText.m_Content.empty() ) return Result<ecs::Entity>::Ok( inE );

    if ( inText.m_FontPath.empty() )
        return Result<ecs::Entity>::Err( make_error_code( errors::UIWidgetError::TextWithNoFont ) );

    if ( auto r = inReg.AddComponent<components::UILabel>( inE, components::UILabel{
             .m_FontPath = inText.m_FontPath, .m_Text = inText.m_Content, .m_Align = inText.m_Align,
             .m_VerticalAlign = inText.m_VerticalAlign, .m_Color = inText.m_Color,
             .m_FontPixelHeight = inText.m_FontPixelHeight, .m_AutoSize = inAutoSize } ); !r )
    {
        return Result<ecs::Entity>::Err( r.Error() );
    }

    return Result<ecs::Entity>::Ok( inE );
}

}

asge::Result<asge::ecs::Entity> 
asge::game::ui::CreateLabel( ecs::Registry& inReg, LabelDesc const& inDesc )
{
    auto entityR = CreateUIEntity( inReg, inDesc.m_Name, inDesc.m_Position,
                                   inDesc.m_Size.value_or( math::Float2{} ), 
                                   inDesc.m_ScreenSpace );

    if ( !entityR ) return entityR;

    ecs::Entity const e = entityR.Value();
    if ( auto r = AddText( inReg, e, inDesc.m_Text, !inDesc.m_Size.has_value() ); !r )
    {
        auto _ = inReg.DestroyEntity( e );
        return r;
    }

    return entityR;
}

asge::Result<asge::ecs::Entity> 
asge::game::ui::CreateButton( ecs::Registry& inReg, ButtonDesc const& inDesc )
{
    auto entityR = CreateUIEntity( inReg, inDesc.m_Name, inDesc.m_Position, inDesc.m_Size, inDesc.m_ScreenSpace );
    if ( !entityR ) return entityR;
    ecs::Entity const e = entityR.Value();

    auto const fail = [&]( auto const& inError ) {
        auto _ = inReg.DestroyEntity( e );
        return Result<ecs::Entity>::Err( inError );
    };

    if ( auto r = inReg.AddComponent<components::Interactable>( e, components::Interactable{
             .m_Enabled = inDesc.m_Enabled } ); !r )
    {
        return fail( r.Error() );
    }

    components::UIButton button{ .m_Colors = inDesc.m_Colors };
    if ( inDesc.m_OnClick ) button.m_OnClick.Connect( inDesc.m_OnClick );
    if ( auto r = inReg.AddComponent<components::UIButton>( e, std::move( button ) ); !r )
        return fail( r.Error() );

    // Button text fills the button's UIRect and is cropped to it: never auto-size
    if ( auto r = AddText( inReg, e, inDesc.m_Text, false ); !r )
        return fail( r.Error() );

    return entityR;
}