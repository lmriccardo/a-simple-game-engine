#pragma once

#include "../../Window.hpp"
#include <string>
#include <SDL3/SDL.h>

namespace asge::graphics
{

/**
 * A owing wrapper around SDL_Window
 */
class SDLWindow final : public IWindow
{
private:
    SDL_Window* m_Window{nullptr};

public:
    SDLWindow(std::string const& inTitle, int inWidth, int inHeight);
    SDLWindow(SDLWindow&& inOther);
    SDLWindow& operator=(SDLWindow&& inOther);

    virtual ~SDLWindow();

    // Window is not copyiable at all
    SDLWindow(SDLWindow const& inOther) = delete;
    SDLWindow& operator=(SDLWindow const& inOther) = delete;

    void* NativeHandle() const override;
    math::Int2 Size() const override;
    bool IsValid() const override;

    // Destroy procedure for the window
    void Destroy();
};

} // namespace asge::graphics
