#include "Transform.hpp"

void asge::game::scene::Serializer<asge::game::components::Transform>::ToToml(
    components::Transform inTransform, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set("m_X", inTransform.m_LocalCoordinates.x())
           .Set("m_Y", inTransform.m_LocalCoordinates.y())
           .Set("m_Rotation", inTransform.m_LocalRotation)
           .Set("m_ScaleX", inTransform.m_LocalScale.x())
           .Set("m_ScaleY", inTransform.m_LocalScale.y());
}

asge::game::components::Transform asge::game::scene::Serializer<asge::game::components::Transform>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));

    components::Transform result{};
    result.m_LocalCoordinates = math::Float2{table.Get( "m_X", 0.0f ), table.Get( "m_Y", 0.0f )};
    result.m_LocalScale = math::Float2{table.Get( "m_ScaleX", 1.0f ), table.Get( "m_ScaleY", 1.0f )};
    result.m_LocalRotation = table.Get("m_Rotation", result.m_LocalRotation);
    return result;
}
