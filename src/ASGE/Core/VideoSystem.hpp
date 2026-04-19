#pragma once

#include <SDL3/SDL.h>
#include <ASGE/Graphics/SDLRenderer.hpp>
#include <memory>
#include <string>

namespace asge
{

class VideoSystem
{
private:
    SDL_Window* m_Window;                            // The main SDL window
    std::unique_ptr<graphics::IRenderer> m_Renderer; // The actual renderer

public:
    inline ~VideoSystem() { Shutdown(); }

    /**
     * @brief Initialize the video system
     * 
     * Initialize the window system by creating the main window and
     * the SDL renderer given title and window dimensions.
     * 
     * @param inTitle The title of the window
     * @param inWidth The width of the window
     * @param inHeight The height of the window
     */
    bool Initialize(std::string const& inTitle, int inWidth, int inHeight);

    // Shutdown the video system closing the renderer and the window
    void Shutdown();

    // Returns a reference to the renderer
    graphics::IRenderer& GetRenderer();

    // Returns a const reference to the renderer
    graphics::IRenderer const& GetRenderer() const;
};

}