#include "SDLWindow.hpp"

using namespace asge::graphics;

asge::graphics::SDLWindow::SDLWindow(std::string const& inTitle, int inWidth, int inHeight)
{
    m_Window = SDL_CreateWindow(inTitle.c_str(), inWidth, inHeight, 0);
    if (!m_Window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation fail : %s\n", SDL_GetError());
    }
}

asge::graphics::SDLWindow::SDLWindow(SDLWindow &&inOther)
: m_Window(inOther.m_Window)
{
    inOther.m_Window = nullptr;
}

SDLWindow &asge::graphics::SDLWindow::operator=(SDLWindow &&inOther)
{
    if ( this != &inOther )
    {
        Destroy(); // First we need to destroy the existing one
        m_Window = inOther.m_Window;
        inOther.m_Window = nullptr;
    }

    return *this;
}

asge::graphics::SDLWindow::~SDLWindow()
{
    Destroy();
}

void* asge::graphics::SDLWindow::NativeHandle() const
{
    return m_Window;
}

asge::math::Int2 asge::graphics::SDLWindow::Size() const
{
    int width{0};
    int height{0};
    SDL_GetWindowSize(m_Window, &width, &height);
    return math::Int2{ width, height };
}

bool asge::graphics::SDLWindow::IsValid() const
{
    return m_Window != nullptr;
}

void asge::graphics::SDLWindow::Destroy()
{
    if (m_Window)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }
}
