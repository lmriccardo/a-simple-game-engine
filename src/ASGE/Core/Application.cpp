#include "Application.hpp"

asge::Application::Application(IGame &inGame, ApplicationConfig const &inConfig)
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
            if (event.type == SDL_EVENT_QUIT) {
                m_Running = false;
            }
        }

        m_Game.Update(0.016f);
        m_Game.Render(m_VideoSys.GetRenderer());
        m_VideoSys.GetRenderer().Present();

        SDL_Delay(16);
    }
}
