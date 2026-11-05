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
    /**
     * @brief Shuts the video system down.
     *
     * Calls Shutdown(), so the renderer, window and backend are released
     * even if the owner never called it explicitly.
     */
    inline ~VideoSystem() { Shutdown(); }

    /**
     * @brief Initialize the video system
     *
     * Brings up the given graphics backend, then creates the main window
     * (inTitle, inWidth x inHeight pixels) and its renderer. On any failure
     * it shuts down whatever was already created and returns an error Result.
     * inResizable defaults to false; asge-editor is the one caller that opts
     * in. The renderer's viewport is NOT kept in sync with a resizable
     * window, so that caller must update it on SDL_EVENT_WINDOW_RESIZED
     * (see IRenderer::SetViewport).
     */
    BoolResult Initialize(std::string const& inTitle, int inWidth, int inHeight,
        GraphicsBackend inBackend = GraphicsBackend::SDL, bool inResizable = false);

    /**
     * @brief Shuts down the video system, closing the renderer, window and backend.
     *
     * Releases the renderer first, then the window, then the backend
     * subsystem. Safe to call repeatedly and on a system that never
     * initialized successfully.
     */
    void Shutdown();

    /**
     * @brief Returns the renderer.
     *
     * Only valid after a successful Initialize(); calling it before, or after
     * Shutdown(), dereferences a null pointer.
     */
    IRenderer& GetRenderer();

    /**
     * @brief Returns the renderer (const overload).
     *
     * Same precondition as the non-const GetRenderer().
     */
    IRenderer const& GetRenderer() const;

    /**
     * @brief Returns the main window.
     *
     * Only valid after a successful Initialize(); calling it before, or after
     * Shutdown(), dereferences a null pointer.
     */
    IWindow& GetWindow();

    /**
     * @brief Returns the main window (const overload).
     *
     * Same precondition as the non-const GetWindow().
     */
    IWindow const& GetWindow() const;
};

}
