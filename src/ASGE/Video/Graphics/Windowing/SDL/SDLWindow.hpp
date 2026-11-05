#pragma once

#include "../../Window.hpp"
#include <string>
#include <SDL3/SDL.h>

namespace asge::video
{

/**
 * @brief An owning wrapper around SDL_Window.
 */
class SDLWindow final : public IWindow
{
private:
    SDL_Window* m_Window{nullptr}; // The owned native window

public:
    /**
     * @brief Creates an SDL window with the given title and pixel size.
     *
     * inResizable adds the resizable flag. A creation failure is logged and
     * leaves IsValid() false.
     */
    SDLWindow(std::string const& inTitle, int inWidth, int inHeight, bool inResizable);

    /**
     * @brief Takes ownership of inOther's SDL window, leaving it empty.
     */
    SDLWindow(SDLWindow&& inOther);

    /**
     * @brief Move-assigns, destroying the current window first.
     *
     * Self-assignment is a no-op; inOther is left empty.
     */
    SDLWindow& operator=(SDLWindow&& inOther);

    /**
     * @brief Destroys the SDL window (see Destroy()).
     */
    virtual ~SDLWindow();

    /**
     * @brief Copy construction is disabled.
     */
    SDLWindow(SDLWindow const& inOther) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    SDLWindow& operator=(SDLWindow const& inOther) = delete;

    /**
     * @brief Returns the owned SDL_Window* as a void* (null when empty).
     */
    void* NativeHandle() const override;

    /**
     * @brief Current window size via SDL_GetWindowSize.
     *
     * A failure is logged and the result is {0, 0}.
     */
    math::Int2 Size() const override;

    /**
     * @brief True while the native SDL window exists.
     */
    bool IsValid() const override;

    /**
     * @brief Destroys the native SDL window.
     *
     * Safe to call more than once; afterwards IsValid() is false.
     */
    void Destroy();
};

} // namespace asge::video
