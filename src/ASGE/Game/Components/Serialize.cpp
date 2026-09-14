#include "Serialize.hpp"

void asge::game::components::Serializer<asge::math::Rect>::ToToml(
    math::Rect inShape, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Set("m_Width",   inShape.w)
           .Set("m_Height",  inShape.h)
           .Set("m_OffsetX", inShape.x)
           .Set("m_OffsetY", inShape.y);
}

asge::math::Rect asge::game::components::Serializer<asge::math::Rect>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    return math::Rect{
        inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f),
        inTview.Get("m_Width", 0.0f), inTview.Get("m_Height", 0.0f)
    };
}

void asge::game::components::Serializer<asge::math::Circle>::ToToml(
    math::Circle inShape, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Set("m_OffsetX", inShape.m_Center.x())
           .Set("m_OffsetY", inShape.m_Center.y())
           .Set("m_Radius",  inShape.m_Radius);
}

asge::math::Circle asge::game::components::Serializer<asge::math::Circle>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    return math::Circle{
        math::Float2{ inTview.Get("m_OffsetX", 0.0f), inTview.Get("m_OffsetY", 0.0f) },
        inTview.Get("m_Radius",  0.0f)
    };
}
