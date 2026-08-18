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
 * and paired with a matching ShutdownBackend call.
 *
 * @param inBackend The graphics backend to initialize
 */
BoolResult InitializeBackend(GraphicsBackend inBackend);

/**
 * @brief Shuts down the given graphics backend's subsystem
 *
 * @param inBackend The graphics backend to shut down
 */
void ShutdownBackend(GraphicsBackend inBackend);

/**
 * @brief Creates a window for the given graphics backend
 *
 * @param inBackend The graphics backend to create the window for
 * @param inTitle The title of the window
 * @param inWidth The width of the window
 * @param inHeight The height of the window
 */
std::unique_ptr<IWindow> CreateWindow(
    GraphicsBackend inBackend, std::string const& inTitle, int inWidth, int inHeight);

/**
 * @brief Creates a renderer for the given graphics backend, bound to the input window
 *
 * @param inBackend The graphics backend to create the renderer for
 * @param inWindow The window the renderer will draw to
 */
std::unique_ptr<IRenderer> CreateRenderer(GraphicsBackend inBackend, IWindow const& inWindow);

} // namespace asge::video
