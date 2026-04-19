#pragma once

#include "VideoSystem.hpp"
#include "Game.hpp"
#include "ApplicationConfig.hpp"

namespace asge
{

class Application
{
private:
    VideoSystem              m_VideoSys;       // The video system that manages the render and window
    bool                     m_Running{false}; // The actual running state of the application  
    IGame&                   m_Game;           // A reference to the input game
    ApplicationConfig const& m_Config;         // The application configuration

public:
    Application(IGame& inGame, ApplicationConfig const& inConfig);

    // Main Application Loop
    void Run();
};

}