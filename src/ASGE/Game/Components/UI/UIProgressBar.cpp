#include "UIProgressBar.hpp"

#include <algorithm>

namespace
{

using asge::game::components::FillDirection;

asge::str::String ToString( FillDirection inValue ) noexcept
{
    switch ( inValue )
    {
    case FillDirection::LeftToRight: return "LeftToRight";
    case FillDirection::RightToLeft: return "RightToLeft";
    case FillDirection::TopToBottom: return "TopToBottom";
    case FillDirection::BottomToTop: return "BottomToTop";
    }

    return "LeftToRight";
}

FillDirection FromString( asge::str::String const& inValue ) noexcept
{
    if ( inValue == "RightToLeft" ) return FillDirection::RightToLeft;
    if ( inValue == "TopToBottom" ) return FillDirection::TopToBottom;
    if ( inValue == "BottomToTop" ) return FillDirection::BottomToTop;
    return FillDirection::LeftToRight;
}

std::int64_t ColorToInt( asge::graphics::RGBA_Color const& inColor ) noexcept
{
    return static_cast<std::int64_t>( asge::graphics::RGBATo32A( inColor ) );
}

asge::graphics::RGBA_Color ColorFromInt( std::int64_t inValue ) noexcept
{
    return asge::graphics::C32AToRGBA( static_cast<asge::graphics::Color32A>( inValue ) );
}

}

void asge::game::scene::Serializer<asge::game::components::UIProgressBar>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    table.Set( "m_Direction", ToString( inValue.m_Direction ) )
         .Set( "m_Min", inValue.m_Min )
         .Set( "m_Max", inValue.m_Max )
         .Set( "m_Start", inValue.m_Start )
         .Set( "m_DefaultStep", inValue.m_DefaultStep )
         .Set( "m_BackgroundColor", ColorToInt( inValue.m_BackgroundColor ) )
         .Set( "m_FillColor", ColorToInt( inValue.m_FillColor ) )
         .Set( "m_BorderColor", ColorToInt( inValue.m_BorderColor ) );
}

asge::game::components::UIProgressBar
asge::game::scene::Serializer<asge::game::components::UIProgressBar>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UIProgressBar result;

    result.m_Direction = FromString( table.Get( "m_Direction", ToString( result.m_Direction ) ) );
    result.m_Min = table.Get( "m_Min", result.m_Min );
    result.m_Max = table.Get( "m_Max", result.m_Max );
    result.m_Start = table.Get( "m_Start", result.m_Start );
    result.m_DefaultStep = table.Get( "m_DefaultStep", result.m_DefaultStep );
    result.m_BackgroundColor = ColorFromInt( table.Get( "m_BackgroundColor", ColorToInt( result.m_BackgroundColor ) ) );
    result.m_FillColor = ColorFromInt( table.Get( "m_FillColor", ColorToInt( result.m_FillColor ) ) );
    result.m_BorderColor = ColorFromInt( table.Get( "m_BorderColor", ColorToInt( result.m_BorderColor ) ) );

    // The runtime value starts where the bar is configured to start.
    result.m_CurrValue = result.m_Start;

    return result;
}

void asge::game::components::Step(UIProgressBar &inBar, float inStep) noexcept
{
    if ( inBar.m_IsAtMax ) return;

    float const newVal = inBar.m_CurrValue + inStep;
    if ( newVal <= inBar.m_Min ) return;
    float const prevVal = inBar.m_CurrValue;
    inBar.m_CurrValue = newVal;
    
    if ( newVal >= inBar.m_Max )
    {
        if ( prevVal < inBar.m_Max )
        {
            inBar.m_OnMaxReached.Emit( inBar.m_Max );
            inBar.m_IsAtMax = true;
        }

        return;
    }

    inBar.m_OnValueChange.Emit( inBar.m_CurrValue );
}

void asge::game::components::Step(UIProgressBar &inBar) noexcept
{
    Step( inBar, inBar.m_DefaultStep );
}
