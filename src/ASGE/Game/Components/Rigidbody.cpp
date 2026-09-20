#include "Rigidbody.hpp"

void asge::game::scene::Serializer<asge::game::components::Rigidbody>::ToToml(
    components::Rigidbody inRigidbody, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set("m_Mass", inRigidbody.m_Mass)
           .Set("m_AffectedByGravity", inRigidbody.m_AffectedByGravity);
}

asge::game::components::Rigidbody asge::game::scene::Serializer<asge::game::components::Rigidbody>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));
    components::Rigidbody result{};
    result.m_Mass = table.Get("m_Mass", result.m_Mass);
    result.m_AffectedByGravity = table.Get("m_AffectedByGravity", result.m_AffectedByGravity);
    return result;
}
