#include "Serialize.hpp"

void asge::game::scene::Serializer<asge::math::Rect>::ToToml(
    math::Rect inShape, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Set("m_Width",   inShape.m_Width)
           .Set("m_Height",  inShape.m_Height)
           .Set("m_OffsetX", inShape.m_X)
           .Set("m_OffsetY", inShape.m_Y);
}

asge::math::Rect asge::game::scene::Serializer<asge::math::Rect>::FromToml(
    asge::config::toml::TOMLTableView inTview) noexcept
{
    return math::Rect{
        inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f),
        inTview.Get("m_Width", 0.0f), inTview.Get("m_Height", 0.0f)
    };
}

void asge::game::scene::Serializer<asge::math::Circle>::ToToml(
    math::Circle inShape, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Set("m_OffsetX", inShape.m_Center.x())
           .Set("m_OffsetY", inShape.m_Center.y())
           .Set("m_Radius",  inShape.m_Radius);
}

asge::math::Circle asge::game::scene::Serializer<asge::math::Circle>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    return math::Circle{
        math::Float2{ inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f) },
        inTview.Get("m_Radius",  0.0f)
    };
}

void asge::game::scene::Serializer<asge::graphics::RGBA_Color>::ToToml(
    graphics::RGBA_Color inColor, asge::config::toml::TOMLTableView inTview) noexcept
{
    inTview.Set( "m_Red",   static_cast<int>( inColor.r ) )
           .Set( "m_Green", static_cast<int>( inColor.g ) )
           .Set( "m_Blue",  static_cast<int>( inColor.b ) )
           .Set( "m_Alpha", static_cast<int>( inColor.a ) );
}

asge::graphics::RGBA_Color asge::game::scene::Serializer<asge::graphics::RGBA_Color>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    graphics::RGBA_Color result;
    result.r = static_cast<std::uint8_t>(inTview.Get<int>( "m_Red",   static_cast<int>(result.r) ));
    result.g = static_cast<std::uint8_t>(inTview.Get<int>( "m_Green", static_cast<int>(result.g) ));
    result.b = static_cast<std::uint8_t>(inTview.Get<int>( "m_Blue",  static_cast<int>(result.b) ));
    result.a = static_cast<std::uint8_t>(inTview.Get<int>( "m_Alpha", static_cast<int>(result.a) ));
    return result;
}
