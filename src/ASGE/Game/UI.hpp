#pragma once

#include <functional>

#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::errors
{

enum class UIWidgetError
{
    TextWithNoFont,
};

inline str::String ToErrorString(UIWidgetError e) noexcept
{
    switch (e)
    {
    case UIWidgetError::TextWithNoFont: "text provided but there is no font path available";
    }
    return "unknown UI widget error";
}

}

REGISTER_ASGE_ERROR(asge::errors::UIWidgetError, "asge.game.ui")

namespace asge::game::ui
{

namespace consts
{

static constexpr int kFontPixelHeight = 16;
static constexpr math::Float2 kButtonSize = math::Float2{80.0f, 24.0f};
static constexpr math::Float2 kCheckboxSize = math::Float2{24.0f, 24.0f};
static constexpr graphics::RGBA_Color kDefaultColor = graphics::colors::s_Black;
static constexpr components::details::StateColors kStateColor =
    components::details::StateColors{};

}

struct TextDesc
{
    str::String          m_Content;
    str::String          m_FontPath;
    int                  m_FontPixelHeight{ consts::kFontPixelHeight };
    str::TextAlign       m_Align{ str::TextAlign::Left };
    components::VerticalAlign m_VerticalAlign{ components::VerticalAlign::Center };
    graphics::RGBA_Color m_Color{ consts::kDefaultColor };
};

struct LabelDesc
{
    str::String                 m_Name;
    math::Float2                m_Position{};
    std::optional<math::Float2> m_Size;              // nullopt: fit the text
    bool                        m_ScreenSpace{ true };
    TextDesc                    m_Text;
};

struct ButtonDesc
{
    str::String                      m_Name;
    bool                             m_Enabled{ true };
    math::Float2                     m_Position{};
    math::Float2                     m_Size{ consts::kButtonSize };
    components::details::StateColors m_Colors{ consts::kStateColor };
    bool                             m_ScreenSpace{ true };
    TextDesc                         m_Text;         // empty content: no label
    std::function<void()>            m_OnClick;
};

struct CheckboxDesc
{
    str::String                      m_Name;
    bool                             m_Enabled{ true };
    math::Float2                     m_Position{};
    math::Float2                     m_Size{ consts::kCheckboxSize };
    components::details::StateColors m_BoxColors{ consts::kStateColor };
    graphics::RGBA_Color             m_CheckColor{ consts::kDefaultColor };
    bool                             m_Checked{ false };
    bool                             m_ScreenSpace{ true };
    std::function<void(bool)>        m_OnToggled;
};

Result<ecs::Entity> CreateLabel( ecs::Registry& inReg, LabelDesc const& inDesc );
Result<ecs::Entity> CreateButton( ecs::Registry& inReg, ButtonDesc const& inDesc );
Result<ecs::Entity> CreateCheckbox( ecs::Registry& inReg, CheckboxDesc const& inDesc );

}