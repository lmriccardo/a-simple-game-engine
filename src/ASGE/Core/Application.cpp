#include "Application.hpp"

using namespace asge;

void Application::processEvent(SDL_Event const* inSdlEventPtr) noexcept
{
    event::SystemEvent sysEvent = event::_internal::ProcessEvent( inSdlEventPtr );

    // Check if the event is a quit event
    if ( auto const* event = sysEvent.TryGet<event::QuitEvent>() )
    {
        LOG_DEBUG("Application quit requested");
        m_Running = false;
        return;
    }
}

asge::Application::Application(game::IGame &inGame, ApplicationConfig const &inConfig)
: m_Game(inGame), m_Config(inConfig)
{
    m_VideoSys.Initialize( inConfig.s_Title, inConfig.s_Width, inConfig.s_Height );
}

void asge::Application::Run()
{
    m_Running = true;
    SDL_Event event;

    while (m_Running)
    {
        while (SDL_PollEvent(&event))
        {
            processEvent( &event );
        }

        m_Game.Update(0.016f);
        m_Game.Render(m_VideoSys.GetRenderer());
        m_VideoSys.GetRenderer().Present();

        SDL_Delay(16);
    }
}
