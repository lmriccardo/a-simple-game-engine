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
    std::array<bool, kKeyCount>     m_KeyCurrentFrame   {}; // Key held state this frame
    std::array<bool, kKeyCount>     m_KeyPreviousFrame  {}; // Key held state last frame
    std::array<bool, kMouseBCount>  m_MouseCurrentFrame {}; // Mouse button state this frame
    std::array<bool, kMouseBCount>  m_MousePreviousFrame{}; // Mouse button state last frame

    math::Float2 m_MousePos         {}; // Latest mouse position (window space)
    math::Float2 m_PreviousMousePos {}; // Mouse position at the last NewFrame()
    math::Float2 m_ScrollDelta      {}; // Wheel scroll accumulated this frame
public:
    /**
     * @brief Default constructor.
     *
     * Defaulted; members take their default values.
     */
    InputState() = default;

    /**
     * @brief Copy constructor.
     *
     * Defaulted; copies each member.
     */
    InputState( InputState const& ) = default;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    InputState( InputState && ) = default;

    /**
     * @brief Copy assignment.
     *
     * Defaulted; assigns each member.
     */
    InputState& operator=( InputState const& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    InputState& operator=( InputState && ) = default;

    /**
     * @brief Destructor.
     *
     * Defaulted; members clean up after themselves.
     */
    ~InputState() = default;

    /**
     * @brief Folds one SystemEvent's key/mouse data into the current-frame state.
     *
     * Keyboard and mouse-button events overwrite the held flag for their
     * key/button, motion events overwrite the mouse position, and wheel events
     * add to the scroll accumulator. Any other event type is ignored, and
     * out-of-range keycodes/buttons are dropped instead of indexing the arrays.
     */
    void Consume( event::SystemEvent const& inEvent ) noexcept;

    /**
     * @brief Rolls the current frame into the previous one and clears the scroll.
     *
     * Call once per frame, before pumping that frame's events. It snapshots the
     * key, button and mouse-position state that Is*Pressed/Is*Released and
     * GetMouseDelta compare against, and resets the scroll accumulator to zero.
     * Skipping it makes "pressed" and "released" report false forever.
     */
    void NewFrame() noexcept;

    /**
     * @brief True while inKey is held down.
     *
     * Level-triggered. inKey must be below Keycode::COUNT; there is no bounds check.
     */
    [[nodiscard]] bool IsKeyDown(Keycode inKey) const noexcept;

    /**
     * @brief True only on the frame inKey went from up to down.
     *
     * Compares the current and previous frame snapshots, so it needs NewFrame()
     * to be called once per frame to be meaningful. A key held across several
     * frames reports true only on the first. inKey must be below Keycode::COUNT.
     */
    [[nodiscard]] bool IsKeyPressed(Keycode inKey) const noexcept;

    /**
     * @brief True only on the frame inKey went from down to up.
     *
     * Compares the current and previous frame snapshots, so it needs NewFrame()
     * to be called once per frame to be meaningful. It reports true for exactly
     * one frame after the key is let go. inKey must be below Keycode::COUNT.
     */
    [[nodiscard]] bool IsKeyReleased(Keycode inKey) const noexcept;

    /**
     * @brief True while inButton is held down.
     *
     * Level-triggered. inButton must be below MouseButton::COUNT; no bounds check.
     */
    [[nodiscard]] bool IsMouseButtonDown(MouseButton inButton) const noexcept;

    /**
     * @brief True only on the frame inButton went from up to down.
     *
     * Compares the current and previous frame snapshots, so it needs NewFrame()
     * to be called once per frame to be meaningful. A button held across several
     * frames reports true only on the first. inButton must be below COUNT.
     */
    [[nodiscard]] bool IsMouseButtonPressed(MouseButton inButton) const noexcept;

    /**
     * @brief True only on the frame inButton went from down to up.
     *
     * Compares the current and previous frame snapshots, so it needs NewFrame()
     * to be called once per frame to be meaningful. It reports true for exactly
     * one frame after the button is let go. inButton must be below COUNT.
     */
    [[nodiscard]] bool IsMouseButtonReleased(MouseButton inButton) const noexcept;

    /**
     * @brief Mouse position, relative to the window.
     *
     * Zero until the first motion event arrives; not reset by NewFrame().
     */
    [[nodiscard]] math::Float2 GetMousePosition() const noexcept;

    /**
     * @brief Mouse movement since the last NewFrame().
     *
     * Computed as the current position minus the position snapshotted by
     * NewFrame(), so it covers every motion event pumped this frame. It is
     * zero on a frame with no motion, and the very first frame can show a
     * jump from the origin to wherever the cursor first appears.
     */
    [[nodiscard]] math::Float2 GetMouseDelta() const noexcept;

    /**
     * @brief Wheel scroll accumulated since the last NewFrame().
     *
     * Sums every wheel event this frame; a per-frame value, not a running total.
     */
    [[nodiscard]] math::Float2 GetScrollDelta() const noexcept;
};

}
