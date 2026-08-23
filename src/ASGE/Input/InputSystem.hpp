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
    InputState m_State;
public:
    InputSystem() = default;

    /** @brief Roll the previous frame's snapshot forward; call before polling this frame's events. */
    void NewFrame() noexcept;

    /** @brief Feed one SystemEvent into the underlying InputState. */
    void ProcessEvent( event::SystemEvent const& inEvent ) noexcept;

    /** @brief Read-only access to the queryable input state. */
    [[nodiscard]] InputState const& GetState() const noexcept;
};

}
