#pragma once

#include "Window.hpp"
#include "Renderer.hpp"
#include <ASGE/Video/GraphicsBackend.hpp>
#include <ASGE/Core/Errors.hpp>
#include <memory>
#include <string>

namespace asge::video
{

/**
 * @brief Initializes the given graphics backend's subsystem
 *
 * Must be called once before CreateWindow/CreateRenderer for that backend,
 * and paired with a matching ShutdownBackend call. For SDL this is
 * SDL_Init(SDL_INIT_VIDEO). Returns an error Result carrying the backend's
 * message on failure, including for an unknown backend value.
 */
BoolResult InitializeBackend(GraphicsBackend inBackend);

/**
 * @brief Shuts down the given graphics backend's subsystem
 *
 * Counterpart of InitializeBackend; for SDL this calls SDL_Quit(), which
 * tears down every SDL subsystem. Destroy all windows, renderers and
 * textures of that backend first.
 */
void ShutdownBackend(GraphicsBackend inBackend);

/**
 * @brief Creates a window for the given graphics backend
 *
 * inWidth/inHeight are in pixels and inResizable lets the user resize it.
 * The backend must already be initialized. Creation failure is logged by
 * the backend and shows up as a window whose IsValid() is false, so check
 * it. Returns nullptr only for an unknown backend value.
 */
std::unique_ptr<IWindow> CreateWindow(
    GraphicsBackend inBackend, std::string const& inTitle, int inWidth, int inHeight, bool inResizable);

/**
 * @brief Creates a renderer for the given graphics backend, bound to the input window
 *
 * inWindow must come from CreateWindow for the same backend and outlive the
 * renderer. As with CreateWindow, failures are logged and give a renderer
 * whose IsValid() is false; nullptr only for an unknown backend value.
 */
std::unique_ptr<IRenderer> CreateRenderer(GraphicsBackend inBackend, IWindow const& inWindow);

} // namespace asge::video
