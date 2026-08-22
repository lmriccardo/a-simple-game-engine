#pragma once

#include <cstddef>
#include <array>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Events/Events.hpp>
#include "Keycode.hpp"
#include "MouseButton.hpp"

namespace asge::input
{

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Keycode::COUNT);
inline constexpr std::size_t kMouseBCount = static_cast<std::size_t>(MouseButton::COUNT);

/**
 * @brief Queryable keyboard/mouse state, built by feeding it the SystemEvent stream.
 *
 * Not a second input source: NewFrame()/Consume() accumulate the same
 * SystemEvents Application::Run already pumps through IGame::OnSystemEvent,
 * so games can poll (IsKeyDown, ...) instead of hand-tracking bools themselves.
 */
class InputState
{
    std::array<bool, kKeyCount>     m_KeyCurrentFrame   {};
    std::array<bool, kKeyCount>     m_KeyPreviousFrame  {};
    std::array<bool, kMouseBCount>  m_MouseCurrentFrame {};
    std::array<bool, kMouseBCount>  m_MousePreviousFrame{};

    math::Float2 m_MousePos         {};
    math::Float2 m_PreviousMousePos {};
    math::Float2 m_ScrollDelta      {};
public:
    InputState() = default;

    InputState( InputState const& ) = default;
    InputState( InputState && ) = default;
    InputState& operator=( InputState const& ) = default;
    InputState& operator=( InputState && ) = default;

    ~InputState() = default;

    /** @brief Folds one SystemEvent's key/mouse data into the current-frame state. */
    void Consume( event::SystemEvent const& inEvent ) noexcept;

    /** @brief Call once per frame, before pumping this frame's events, to roll current into previous. */
    void NewFrame() noexcept;

    /** @brief True while inKey is held down. */
    [[nodiscard]] bool IsKeyDown(Keycode inKey) const noexcept;
    /** @brief True only on the frame inKey went from up to down. */
    [[nodiscard]] bool IsKeyPressed(Keycode inKey) const noexcept;
    /** @brief True only on the frame inKey went from down to up. */
    [[nodiscard]] bool IsKeyReleased(Keycode inKey) const noexcept;

    /** @brief True while inButton is held down. */
    [[nodiscard]] bool IsMouseButtonDown(MouseButton inButton) const noexcept;
    /** @brief True only on the frame inButton went from up to down. */
    [[nodiscard]] bool IsMouseButtonPressed(MouseButton inButton) const noexcept;
    /** @brief True only on the frame inButton went from down to up. */
    [[nodiscard]] bool IsMouseButtonReleased(MouseButton inButton) const noexcept;

    /** @brief Mouse position, relative to the window. */
    [[nodiscard]] math::Float2 GetMousePosition() const noexcept;
    /** @brief Mouse movement since the last NewFrame(). */
    [[nodiscard]] math::Float2 GetMouseDelta() const noexcept;
    /** @brief Wheel scroll accumulated since the last NewFrame(). */
    [[nodiscard]] math::Float2 GetScrollDelta() const noexcept;
};

}