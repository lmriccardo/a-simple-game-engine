#include "SDLWindow.hpp"
#include "../../../VideoError.hpp"

using namespace asge::video;

asge::video::SDLWindow::SDLWindow(std::string const& inTitle, int inWidth, int inHeight)
{
    m_Window = SDL_CreateWindow(inTitle.c_str(), inWidth, inHeight, 0);
    if (!m_Window)
    {
        LogError( make_error_code( errors::VideoError::WindowCreationFailed ), SDL_GetError() );
    }
}

asge::video::SDLWindow::SDLWindow(SDLWindow &&inOther)
: m_Window(inOther.m_Window)
{
    inOther.m_Window = nullptr;
}

SDLWindow &asge::video::SDLWindow::operator=(SDLWindow &&inOther)
{
    if ( this != &inOther )
    {
        Destroy(); // First we need to destroy the existing one
        m_Window = inOther.m_Window;
        inOther.m_Window = nullptr;
    }

    return *this;
}

asge::video::SDLWindow::~SDLWindow()
{
    Destroy();
}

void* asge::video::SDLWindow::NativeHandle() const
{
    return m_Window;
}

asge::math::Int2 asge::video::SDLWindow::Size() const
{
    int width{0};
    int height{0};
    if (!SDL_GetWindowSize(m_Window, &width, &height))
    {
        LogError( make_error_code( errors::VideoError::GetWindowSizeFailed ), SDL_GetError() );
    }

    return math::Int2{ width, height };
}

bool asge::video::SDLWindow::IsValid() const
{
    return m_Window != nullptr;
}

void asge::video::SDLWindow::Destroy()
{
    if (m_Window)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }
}
