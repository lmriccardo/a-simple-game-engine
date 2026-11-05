#pragma once

#include <functional>

#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/UIPanel.hpp>
#include <ASGE/Game/Components/UI/UIProgressBar.hpp>
#include <ASGE/Game/Components/UI/UIDebugOverlay.hpp>
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

/**
 * @brief Human-readable message for a UIWidgetError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
inline str::String ToErrorString(UIWidgetError e) noexcept
{
    switch (e)
    {
    case UIWidgetError::TextWithNoFont: return "text provided but there is no font path available";
    }
    return "unknown UI widget error";
}

}

REGISTER_ASGE_ERROR(asge::errors::UIWidgetError, "asge.game.ui")

namespace asge::game::ui
{

// Defaults used by the Desc structs below.
namespace consts
{

static constexpr int kFontPixelHeight = 16; // Default font bake height
static constexpr math::Float2 kButtonSize = math::Float2{80.0f, 24.0f}; // Default button size
static constexpr math::Float2 kCheckboxSize = math::Float2{24.0f, 24.0f}; // Default checkbox size
static constexpr math::Float2 kPanelSize = math::Float2{200.0f, 120.0f}; // Default panel size
static constexpr math::Float2 kSliderSize = math::Float2{160.0f, 24.0f}; // Default slider size
static constexpr math::Float2 kProgressBarSize = math::Float2{160.0f, 16.0f}; // Default progress bar size
static constexpr graphics::RGBA_Color kDefaultColor = graphics::colors::s_Black; // Default text and check color
static constexpr components::details::StateColors kStateColor = // Default button and thumb colors
    components::details::StateColors{};

}

/**
 * @brief Describes the optional text of a widget.
 *
 * Used by labels and buttons; an empty m_Content means no UILabel is added.
 */
struct TextDesc
{
    str::String          m_Content;                                   // Text to show; empty adds no label
    str::String          m_FontPath;                                  // VFS path of the font; required when there is text
    int                  m_FontPixelHeight{ consts::kFontPixelHeight }; // Pixel height the font is baked at
    str::TextAlign       m_Align{ str::TextAlign::Left };             // Horizontal text alignment
    components::VerticalAlign m_VerticalAlign{ components::VerticalAlign::Center }; // Vertical text alignment
    bool                 m_WordWrap{ false };   // wraps to the label's size; ignored when auto-sized
    graphics::RGBA_Color m_Color{ consts::kDefaultColor };            // Text color
};

/**
 * @brief Describes a text label for CreateLabel().
 *
 * Leave m_Size unset to size the label to its text.
 */
struct LabelDesc
{
    str::String                 m_Name;                               // Entity name; empty adds no Name component
    math::Float2                m_Position{};                         // Top-left position of the widget
    std::optional<math::Float2> m_Size{};            // nullopt: fit the text
    bool                        m_ScreenSpace{ true };                // Draw in screen space instead of world space
    TextDesc                    m_Text;                               // Optional caption
};

/**
 * @brief Describes a clickable button for CreateButton().
 */
struct ButtonDesc
{
    str::String                      m_Name;                          // Entity name; empty adds no Name component
    bool                             m_Enabled{ true };               // Whether the widget reacts to the pointer
    math::Float2                     m_Position{};                    // Top-left position of the widget
    math::Float2                     m_Size{ consts::kButtonSize };   // Width and height of the widget
    components::details::StateColors m_Colors{ consts::kStateColor }; // Fill colors for normal, hover and pressed
    bool                             m_ScreenSpace{ true };           // Draw in screen space instead of world space
    TextDesc                         m_Text;         // empty content: no label
    std::function<void()>            m_OnClick;                       // Connected to the click signal; may be empty
};

/**
 * @brief Describes a toggleable checkbox for CreateCheckbox().
 */
struct CheckboxDesc
{
    str::String                      m_Name;                          // Entity name; empty adds no Name component
    bool                             m_Enabled{ true };               // Whether the widget reacts to the pointer
    math::Float2                     m_Position{};                    // Top-left position of the widget
    math::Float2                     m_Size{ consts::kCheckboxSize }; // Width and height of the widget
    components::details::StateColors m_BoxColors{ consts::kStateColor }; // Box fill colors for normal, hover and pressed
    graphics::RGBA_Color             m_CheckColor{ consts::kDefaultColor }; // Color of the check mark
    bool                             m_Checked{ false };              // Initial checked state
    bool                             m_ScreenSpace{ true };           // Draw in screen space instead of world space
    std::function<void(bool)>        m_OnToggled;                     // Connected to the toggle signal; may be empty
};

/**
 * @brief Describes a draggable slider for CreateSlider().
 */
struct SliderDesc
{
    str::String                      m_Name;                          // Entity name; empty adds no Name component
    bool                             m_Enabled{ true };               // Whether the widget reacts to the pointer
    math::Float2                     m_Position{};                    // Top-left position of the widget
    math::Float2                     m_Size{ consts::kSliderSize };   // Width and height of the widget
    float                            m_Min{ 0.0f };                   // Value at the empty/left end
    float                            m_Max{ 1.0f };                   // Value at the full/right end
    float                            m_Value{ 0.0f };                 // Initial value
    graphics::RGBA_Color             m_TrackColor{ graphics::colors::s_Gray }; // Color of the track
    components::details::StateColors m_ThumbColor{ consts::kStateColor }; // Thumb colors for normal, hover and pressed
    bool                             m_ScreenSpace{ true };           // Draw in screen space instead of world space
    std::function<void(float)>       m_OnValueChanged;                // Connected to the value-change signal; may be empty
};

/**
 * @brief Describes a background panel for CreatePanel().
 */
struct PanelDesc
{
    str::String                m_Name;                                // Entity name; empty adds no Name component
    math::Float2               m_Position{};                          // Top-left position of the widget
    math::Float2               m_Size{ consts::kPanelSize };          // Width and height of the widget
    graphics::RGBA_Color       m_Background{ graphics::colors::s_ShadowBlack }; // Fill color
    components::LayoutSpec     m_Layout{ components::LayoutAbsolute{} }; // How children are placed
    math::Float2               m_Padding{};                           // Inner gap before the children
    math::Float2               m_Margin{};                            // Outer gap before the fill
    math::Float2               m_Spacing{};                           // Gap between neighbouring slots
    bool                       m_Border{ true };                      // Whether an outline is drawn
    graphics::RGBA_Color       m_BorderColor{ graphics::colors::s_LightGray }; // Outline color
    bool                       m_ScreenSpace{ true };                 // Draw in screen space instead of world space
    int                        m_Layer{ -1 };            // behind default-layer widgets
};

/**
 * @brief Describes a progress bar for CreateProgressBar().
 */
struct ProgressBarDesc
{
    str::String                      m_Name;                          // Entity name; empty adds no Name component
    math::Float2                     m_Position{};                    // Top-left position of the widget
    math::Float2                     m_Size{ consts::kProgressBarSize }; // Width and height of the widget
    components::FillDirection        m_Direction{ components::FillDirection::LeftToRight }; // Direction the fill grows
    float                            m_Min{ 0.0f };                   // Value at the empty/left end
    float                            m_Max{ 1.0f };                   // Value at the full/right end
    float                            m_Start{ 0.0f };                 // Value the bar starts at
    float                            m_DefaultStep{ 0.1f };           // Amount Step(bar) advances by
    graphics::RGBA_Color             m_BackgroundColor{ graphics::colors::s_DarkGray }; // Color of the empty part
    graphics::RGBA_Color             m_FillColor{ graphics::colors::s_Magenta }; // Color of the filled part
    graphics::RGBA_Color             m_BorderColor{ graphics::colors::s_LightGray }; // Outline color
    bool                             m_ScreenSpace{ true };           // Draw in screen space instead of world space
    std::function<void(float)>       m_OnValueChange;                 // Connected to the value-change signal; may be empty
    std::function<void(float)>       m_OnMaxReached;                  // Connected to the max-reached signal; may be empty
};

/**
 * @brief Describes the FPS / entity count overlay for CreateDebugOverlay().
 */
struct DebugOverlayDesc
{
    str::String          m_Name{ "DebugOverlay" };                    // Entity name; empty adds no Name component
    str::String          m_FontPath;                                  // VFS path of the font; required when there is text
    math::Float2         m_Position{ 8.0f, 8.0f };                    // Top-left position of the widget
    float                m_RefreshInterval{ 0.25f };                  // Seconds between text updates
    graphics::RGBA_Color m_Color{ graphics::colors::s_White };        // Text color
};

/**
 * @brief Creates a text label entity (UIRect + UILabel).
 *
 * Fails with UIWidgetError::TextWithNoFont if there is text but no font path.
 * The entity is destroyed again if any component fails to attach.
 */
Result<ecs::Entity> CreateLabel( ecs::Registry& inReg, LabelDesc const& inDesc );

/**
 * @brief Creates a clickable button entity (UIRect + Interactable + UIButton).
 *
 * A UILabel is added only when the description has text, and it is never
 * auto-sized: the caption is cropped to the button. The entity is destroyed
 * again if any component fails to attach.
 */
Result<ecs::Entity> CreateButton( ecs::Registry& inReg, ButtonDesc const& inDesc );

/**
 * @brief Creates a toggleable checkbox entity (UIRect + Interactable + UICheckbox).
 *
 * The entity is destroyed again if any component fails to attach.
 */
Result<ecs::Entity> CreateCheckbox( ecs::Registry& inReg, CheckboxDesc const& inDesc );

/**
 * @brief Creates a draggable slider entity (UIRect + Interactable + UISlider).
 *
 * The entity is destroyed again if any component fails to attach.
 */
Result<ecs::Entity> CreateSlider( ecs::Registry& inReg, SliderDesc const& inDesc );

/**
 * @brief Creates a non-interactive panel entity (UIRect + UIPanel).
 *
 * It is drawn as a background, optionally bordered, at the description's
 * m_Layer.
 */
Result<ecs::Entity> CreatePanel( ecs::Registry& inReg, PanelDesc const& inDesc );

/**
 * @brief Creates a non-interactive progress bar entity (UIRect + UIProgressBar).
 *
 * The bar starts at m_Start. The entity is destroyed again if any component
 * fails to attach.
 */
Result<ecs::Entity> CreateProgressBar( ecs::Registry& inReg, ProgressBarDesc const& inDesc );

/**
 * @brief Creates a screen-space label (UILabel + UIDebugOverlay) showing FPS and entity count.
 *
 * Drive it with systems::UIDebugOverlaySystem.
 */
Result<ecs::Entity> CreateDebugOverlay( ecs::Registry& inReg, DebugOverlayDesc const& inDesc );

}
