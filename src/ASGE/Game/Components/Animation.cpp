#include "Animation.hpp"

void asge::game::components::PlayAnimation( Animation& inAnim, bool inLoop ) noexcept
{
    inAnim.m_Playing = true;
    inAnim.m_Loop = inLoop;
}

void asge::game::components::StopAnimation( Animation& inAnim ) noexcept
{
    inAnim.m_Playing = false;
}

void asge::game::components::Serializer<asge::game::components::Animation>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set<std::string>("m_ClipPath", inValue.m_ClipPath)
           .Set("m_FrameDuration", inValue.m_FrameDuration);
}

asge::game::components::Animation asge::game::components::Serializer<asge::game::components::Animation>::FromToml(
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto table = inTview.Table(std::string(kTableName));

    Animation result{};
    result.m_ClipPath      = table.Get<std::string>("m_ClipPath", std::string{});
    result.m_FrameDuration = table.Get("m_FrameDuration", result.m_FrameDuration);
    return result;
}
