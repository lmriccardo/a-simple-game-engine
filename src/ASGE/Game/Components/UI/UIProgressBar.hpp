#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

enum class FillDirection
{
    LeftToRight,
    RightToLeft,
    TopToBottom,
    BottomToTop
};

struct UIProgressBar
{
    using Color  = graphics::RGBA_Color;
    using Signal = signals::Signal<float>;

    // Serializable fields
    FillDirection   m_Direction         { FillDirection::LeftToRight };
    float           m_Min               { 0.0f };
    float           m_Max               { 1.0f };
    float           m_Start             { 0.0f };
    float           m_DefaultStep       { 0.1f };
    Color           m_BackgroundColor   { graphics::colors::s_DarkGray };
    Color           m_FillColor         { graphics::colors::s_Magenta };
    Color           m_BorderColor       { graphics::colors::s_LightGray };

    // Runtime-only fields
    float m_CurrValue   { 0.0f };
    bool  m_IsAtMax     { false };

    Signal m_OnValueChange{};
    Signal m_OnMaxReached{};
};

void Step( UIProgressBar& inBar, float inStep ) noexcept;
void Step( UIProgressBar& inBar ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UIProgressBar>
{
    static constexpr str::StringView kTableName = "UIProgressBar";
    using T = components::UIProgressBar;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}