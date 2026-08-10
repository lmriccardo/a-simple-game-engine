#include "GraphicsFactory.hpp"
#include "../VideoError.hpp"
#include "Windowing/SDL/SDLWindow.hpp"
#include "Rendering/SDLRenderer/SDLRenderer.hpp"

using namespace asge::graphics;

asge::BoolResult asge::graphics::InitializeBackend(video::GraphicsBackend inBackend)
{
    switch (inBackend)
    {
    case video::GraphicsBackend::SDL:
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            return BoolResult::Err(make_error_code(errors::VideoError::SubsystemInitFailed), SDL_GetError());
        }
        return BoolResult::Ok();
    }

    return BoolResult::Err(make_error_code(errors::VideoError::SubsystemInitFailed), "unknown graphics backend");
}

void asge::graphics::ShutdownBackend(video::GraphicsBackend inBackend)
{
    switch (inBackend)
    {
    case video::GraphicsBackend::SDL:
        SDL_Quit();
        return;
    }
}

std::unique_ptr<IWindow> asge::graphics::CreateWindow(
    video::GraphicsBackend inBackend, std::string const& inTitle, int inWidth, int inHeight)
{
    switch (inBackend)
    {
    case video::GraphicsBackend::SDL:
        return std::make_unique<SDLWindow>(inTitle, inWidth, inHeight);
    }

    return nullptr;
}

std::unique_ptr<IRenderer> asge::graphics::CreateRenderer(video::GraphicsBackend inBackend, IWindow const& inWindow)
{
    switch (inBackend)
    {
    case video::GraphicsBackend::SDL:
        return std::make_unique<SDLRenderer>(static_cast<SDL_Window*>(inWindow.NativeHandle()));
    }

    return nullptr;
}
