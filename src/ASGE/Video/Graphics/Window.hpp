#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::graphics
{

class IWindow
{
public:
    virtual ~IWindow() = default;

    // Returns the backend-native window handle (e.g. SDL_Window*)
    virtual void* NativeHandle() const = 0;

    // Returns the current window size, in pixels
    virtual math::Int2 Size() const = 0;

    // Checks if the current window is valid or not
    virtual bool IsValid() const = 0;
};

} // namespace asge::graphics
