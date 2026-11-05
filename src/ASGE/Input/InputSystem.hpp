#pragma once

#include "InputState.hpp"

namespace asge::input
{

/**
 * @brief Owns an InputState and drives it from the frame loop.
 *
 * The piece Application actually holds, mirroring VideoSystem's shape:
 * NewFrame() once per frame before polling, ProcessEvent() for each polled
 * SystemEvent, GetState() for games to query.
 */
class InputSystem
{
    InputState m_State; // Input state fed by the frame loop
public:
    InputSystem() = default;

    /**
     * @brief Rolls the previous frame's snapshot forward.
     *
     * Call once per frame, before polling that frame's events. It simply
     * forwards to InputState::NewFrame(), so the pressed/released edges and
     * the mouse/scroll deltas are measured against the frame just finished.
     */
    void NewFrame() noexcept;

    /**
     * @brief Feeds one SystemEvent into the underlying InputState.
     *
     * Call it for every event polled this frame, after NewFrame(). It forwards
     * to InputState::Consume(), which ignores event types that carry no
     * keyboard or mouse data, so it is safe to pass the whole event stream.
     */
    void ProcessEvent( event::SystemEvent const& inEvent ) noexcept;

    /**
     * @brief Read-only access to the queryable input state.
     *
     * Valid for the InputSystem's lifetime; reflects events processed so far this frame.
     */
    [[nodiscard]] InputState const& GetState() const noexcept;
};

}
