#include "Transform.hpp"

void asge::game::scene::Serializer<asge::game::components::Transform>::ToToml(
    components::Transform inTransform, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set("m_X", inTransform.m_X)
           .Set("m_Y", inTransform.m_Y)
           .Set("m_Rotation", inTransform.m_Rotation)
           .Set("m_ScaleX", inTransform.m_ScaleX)
           .Set("m_ScaleY", inTransform.m_ScaleY);
}

asge::game::components::Transform asge::game::scene::Serializer<asge::game::components::Transform>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));

    components::Transform result{};
    result.m_X        = table.Get("m_X", result.m_X);
    result.m_Y        = table.Get("m_Y", result.m_Y);
    result.m_Rotation = table.Get("m_Rotation", result.m_Rotation);
    result.m_ScaleX   = table.Get("m_ScaleX", result.m_ScaleX);
    result.m_ScaleY   = table.Get("m_ScaleY", result.m_ScaleY);
    return result;
}
