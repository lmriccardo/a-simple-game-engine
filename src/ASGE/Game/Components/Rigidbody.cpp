#include "Rigidbody.hpp"

void asge::game::components::Serializer<asge::game::components::Rigidbody>::ToToml(
    Rigidbody inRigidbody, asge::config::toml::TOMLTableView inTview, scene::SaveContext const& inCtx ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set("m_Mass", inRigidbody.m_Mass)
           .Set("m_AffectedByGravity", inRigidbody.m_AffectedByGravity);
}

asge::game::components::Rigidbody asge::game::components::Serializer<asge::game::components::Rigidbody>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, scene::LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));
    Rigidbody result{};
    result.m_Mass = table.Get("m_Mass", result.m_Mass);
    result.m_AffectedByGravity = table.Get("m_AffectedByGravity", result.m_AffectedByGravity);
    return result;
}
