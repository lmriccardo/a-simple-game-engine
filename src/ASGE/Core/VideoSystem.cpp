#include "VideoSystem.hpp"

using namespace asge;

void asge::VideoSystem::Shutdown()
{
    // Destroy the renderer (its deconstructor calls destroy it)
    m_Renderer.reset();
    if (m_Window)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    SDL_Quit();
}

bool asge::VideoSystem::Initialize(std::string const &inTitle, int inWidth, int inHeight)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    // Create the main window. If creation was not successful returns false
    m_Window = SDL_CreateWindow( inTitle.c_str(), inWidth, inHeight, 0 );
    if (!m_Window)
    {
        SDL_Quit();
        return false;
    }

    // Create the renderer and check for its validity
    m_Renderer = std::make_unique<graphics::SDLRenderer>(m_Window);
    if (!m_Renderer->IsValid())
    {
        SDL_DestroyWindow(m_Window);
        SDL_Quit();
        m_Window = nullptr;
        return false;
    }

    return true;
}

graphics::IRenderer &asge::VideoSystem::GetRenderer()
{
    return *m_Renderer;
}

graphics::IRenderer const &asge::VideoSystem::GetRenderer() const
{
    return *m_Renderer;
}
