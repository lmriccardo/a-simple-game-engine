#pragma once

#include <ASGE/Game/Game.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Logger/Logger.hpp>
#include <ASGE/Video/VideoSystem.hpp>
#include "ApplicationConfig.hpp"

namespace asge
{

class Application
{
private:
    video::VideoSystem       m_VideoSys;       // The video system that manages the render and window
    bool                     m_Running{false}; // The actual running state of the application  
    game::IGame&             m_Game;           // A reference to the input game
    ApplicationConfig const& m_Config;         // The application configuration

    void processEvent(SDL_Event const* inSdlEventPtr) noexcept;

public:
    Application(game::IGame& inGame, ApplicationConfig const& inConfig);

    /* Main application loop */
    void Run();
};

}