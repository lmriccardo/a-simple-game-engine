#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::video
{

class IWindow
{
public:
    virtual ~IWindow() = default;

    /**
     * @brief Returns the backend-native window handle (e.g. SDL_Window*).
     *
     * The window keeps ownership; cast it to the backend type. Null once
     * the window is destroyed.
     */
    virtual void* NativeHandle() const = 0;

    /**
     * @brief Returns the current window size, in pixels.
     *
     * Queried live from the backend, so it follows user resizes. Backend
     * failures are logged and yield {0, 0}.
     */
    virtual math::Int2 Size() const = 0;

    /**
     * @brief Checks if the current window is valid or not.
     *
     * False if creation failed or the window was destroyed.
     */
    virtual bool IsValid() const = 0;
};

} // namespace asge::video
