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

void asge::game::scene::Serializer<asge::game::components::Animation>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(std::string(kTableName))
           .Set<std::string>("m_ClipPath", inValue.m_ClipPath)
           .Set("m_FrameDuration", inValue.m_FrameDuration)
           .Set("m_Loop", inValue.m_Loop)
           .Set("m_StartPlaying", inValue.m_StartPlaying);
}

asge::game::components::Animation asge::game::scene::Serializer<asge::game::components::Animation>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table(std::string(kTableName));

    T result{};
    result.m_ClipPath      = table.Get<std::string>("m_ClipPath", std::string{});
    result.m_FrameDuration = table.Get("m_FrameDuration", result.m_FrameDuration);
    result.m_Loop          = table.Get("m_Loop", result.m_Loop);
    result.m_StartPlaying  = table.Get("m_StartPlaying", result.m_StartPlaying);
    result.m_Playing       = result.m_StartPlaying;
    return result;
}
