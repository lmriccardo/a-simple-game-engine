#pragma once

#include <memory>
#include <string>

#include <ASGE/Core/Errors.hpp>
#include "GraphicsBackend.hpp"
#include "Graphics/Window.hpp"
#include "Graphics/Renderer.hpp"

namespace asge::video
{

class VideoSystem
{
private:
    std::unique_ptr<IWindow>   m_Window;                    // The main window
    std::unique_ptr<IRenderer> m_Renderer;                  // The actual renderer
    GraphicsBackend            m_Backend{GraphicsBackend::SDL}; // The backend in use
    bool                       m_BackendInitialized{false}; // Whether the backend subsystem is up

public:
    inline ~VideoSystem() { Shutdown(); }

    /**
     * @brief Initialize the video system
     *
     * Initialize the window system by bringing up the given graphics
     * backend and creating the main window and renderer for it.
     *
     * @param inTitle The title of the window
     * @param inWidth The width of the window
     * @param inHeight The height of the window
     * @param inBackend The graphics backend to initialize (defaults to SDL)
     */
    BoolResult Initialize(std::string const& inTitle, int inWidth, int inHeight,
        GraphicsBackend inBackend = GraphicsBackend::SDL);

    // Shutdown the video system closing the renderer, the window and the backend subsystem
    void Shutdown();

    // Returns a reference to the renderer
    IRenderer& GetRenderer();

    // Returns a const reference to the renderer
    IRenderer const& GetRenderer() const;
};

}
