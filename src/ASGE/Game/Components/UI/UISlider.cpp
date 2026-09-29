#include "UISlider.hpp"

void asge::game::scene::Serializer<asge::game::components::UISlider>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    Serializer<components::details::StateColors>::ToToml( inValue.m_ThumbColor, table, inCtx );
    table.Set("m_Min", inValue.m_Min);
    table.Set("m_Max", inValue.m_Max);
    table.Set("m_Value", inValue.m_Value);
    table.Set("m_TrackColor", static_cast<std::int64_t>(graphics::RGBATo32A(inValue.m_TrackColor)));
}

asge::game::components::UISlider
asge::game::scene::Serializer<asge::game::components::UISlider>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UISlider result;

    result.m_ThumbColor = Serializer<components::details::StateColors>::FromToml( table, inCtx );
    result.m_Max = table.Get( "m_Max", result.m_Max );
    result.m_Min = table.Get( "m_Min", result.m_Min );
    result.m_Value = table.Get( "m_Value", result.m_Value );

    auto const dColor = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_TrackColor ) );
    result.m_TrackColor = graphics::C32AToRGBA( 
        static_cast<graphics::Color32A>( table.Get( "m_TrackColor", dColor ) ) );

    return result;
}

float asge::game::components::GetStep(UISlider const &inSlider, float inWidth) noexcept
{
    return ( inSlider.m_Max - inSlider.m_Min ) / inWidth;
}

asge::math::Float2 asge::game::components::GetThumbPosition(UISlider const &inSlider, math::Rect inOrigin) noexcept
{
    float const range = inSlider.m_Max - inSlider.m_Min;
    float const t = range > 0.0f
        ? std::clamp( ( inSlider.m_Value - inSlider.m_Min ) / range, 0.0f, 1.0f ) : 0.0f;

    // Thumb center: proportional along the width, vertically mid-rect.
    return math::Float2{ inOrigin.m_X + t * inOrigin.m_Width, inOrigin.m_Y + inOrigin.m_Height / 2.0f };
}
