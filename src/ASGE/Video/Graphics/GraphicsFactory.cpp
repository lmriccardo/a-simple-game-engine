#include "GraphicsFactory.hpp"
#include "../VideoError.hpp"
#include "Windowing/SDL/SDLWindow.hpp"
#include "Rendering/SDL/SDLRenderer.hpp"

using namespace asge::video;

asge::BoolResult asge::video::InitializeBackend(GraphicsBackend inBackend)
{
    switch (inBackend)
    {
    case GraphicsBackend::SDL:
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            return BoolResult::Err(make_error_code(errors::VideoError::SubsystemInitFailed), SDL_GetError());
        }
        return BoolResult::Ok();
    }

    return BoolResult::Err(make_error_code(errors::VideoError::SubsystemInitFailed), "unknown graphics backend");
}

void asge::video::ShutdownBackend(GraphicsBackend inBackend)
{
    switch (inBackend)
    {
    case GraphicsBackend::SDL:
        SDL_Quit();
        return;
    }
}

std::unique_ptr<IWindow> asge::video::CreateWindow(
    GraphicsBackend inBackend, std::string const& inTitle, int inWidth, int inHeight)
{
    switch (inBackend)
    {
    case GraphicsBackend::SDL:
        return std::make_unique<SDLWindow>(inTitle, inWidth, inHeight);
    }

    return nullptr;
}

std::unique_ptr<IRenderer> asge::video::CreateRenderer(GraphicsBackend inBackend, IWindow const& inWindow)
{
    switch (inBackend)
    {
    case GraphicsBackend::SDL:
        return std::make_unique<SDLRenderer>(static_cast<SDL_Window*>(inWindow.NativeHandle()));
    }

    return nullptr;
}
