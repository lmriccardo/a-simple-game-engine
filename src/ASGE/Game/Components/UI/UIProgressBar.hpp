#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Direction in which a UIProgressBar's fill grows as its value increases.
 *
 * Named for where the fill starts and where it ends.
 */
enum class FillDirection
{
    LeftToRight,
    RightToLeft,
    TopToBottom,
    BottomToTop
};

/**
 * @brief UI component for a bar that fills between a minimum and a maximum value.
 *
 * The serializable fields configure range, start value, step and colors; the
 * runtime fields track the current value and expose signals for value changes
 * and for reaching the maximum.
 */
struct UIProgressBar
{
    using Color  = graphics::RGBA_Color;
    using Signal = signals::Signal<float>;

    // Serializable fields
    FillDirection   m_Direction         { FillDirection::LeftToRight };          // Direction the fill grows
    float           m_Min               { 0.0f };                                // Value of an empty bar
    float           m_Max               { 1.0f };                                // Value of a full bar
    float           m_Start             { 0.0f };                                // Value the bar starts at
    float           m_DefaultStep       { 0.1f };                                // Amount Step(bar) advances by
    Color           m_BackgroundColor   { graphics::colors::s_DarkGray };        // Color of the empty part
    Color           m_FillColor         { graphics::colors::s_Magenta };         // Color of the filled part
    Color           m_BorderColor       { graphics::colors::s_LightGray };       // Outline color

    // Runtime-only fields
    float m_CurrValue   { 0.0f };  // Current value, between m_Min and m_Max
    bool  m_IsAtMax     { false }; // Whether the maximum was reached

    Signal m_OnValueChange{};      // Emitted with the new value after each change
    Signal m_OnMaxReached{};       // Emitted with the value when m_Max is reached
};

/**
 * @brief Advances the bar by @p inStep and emits m_OnValueChange with the new value.
 *
 * The value is clamped to [m_Min, m_Max]; nothing happens if it doesn't change or the
 * maximum was already reached. On reaching m_Max, m_OnMaxReached is emitted instead.
 */
void Step( UIProgressBar& inBar, float inStep ) noexcept;

/**
 * @brief Advances the bar by its m_DefaultStep.
 *
 * See the overload taking an explicit step.
 */
void Step( UIProgressBar& inBar ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UIProgressBar>
{
    static constexpr str::StringView kTableName = "UIProgressBar"; // Name of the TOML subtable (de)serialization uses
    using T = components::UIProgressBar;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}