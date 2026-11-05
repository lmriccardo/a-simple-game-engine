#pragma once

#include <optional>

#include <ASGE/Game/Game.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Input/InputSystem.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Audio/AudioDevice.hpp>

#include "ApplicationConfig.hpp"

namespace asge
{

/**
 * @brief Owns TGame and drives the main loop: pump SDL events, update, render, present.
 *
 * TGame is constructed in-place (not passed in) from the IRenderer the owned
 * VideoSystem creates plus whatever extra constructor arguments the caller
 * supplies — TGame must derive from some game::Game<TStateId> (enforced via
 * game::is_game_v) since that's what supplies the matching constructor.
 * m_Game stays empty if VideoSystem::Initialize fails, and Run() refuses to
 * start rather than looping over a game that was never constructed.
 */
template<typename TGame>
requires game::is_game_v<TGame>
class Application
{
private:
    video::VideoSystem       m_VideoSys;       // The video system that manages the render and window
    input::InputSystem       m_InputSys;       // Queryable keyboard/mouse state, fed from the event stream
    audio::AudioDevice       m_AudioDev;       // Audio device for playing music
    bool                     m_Running{false}; // The actual running state of the application
    ApplicationConfig const& m_Config;         // The application configuration
    std::optional<TGame>     m_Game;           // The owned game instance; empty if video init failed

    /**
     * @brief Feeds one SDL event into input tracking and the game.
     *
     * The raw SDL event is translated to a SystemEvent, handed to the
     * InputSystem first so polling state is current, then forwarded to the
     * game's OnSystemEvent. A QuitEvent is consumed here instead: it clears
     * m_Running and is not forwarded to the game. Assumes m_Game is engaged.
     */
    void processEvent(SDL_Event const* inSdlEventPtr) noexcept
    {
        event::SystemEvent sysEvent = event::_internal::ProcessEvent( inSdlEventPtr );
        m_InputSys.ProcessEvent( sysEvent );

        if ( auto const* event = sysEvent.TryGet<event::QuitEvent>() )
        {
            LOG_DEBUG("Application quit requested");
            m_Running = false;
            return;
        }

        m_Game->OnSystemEvent( sysEvent );
    }

public:
    /**
     * @brief Initializes video and audio from inConfig, then constructs TGame.
     *
     * TGame is built in place from the VideoSystem's renderer, the audio
     * device and inGameArgs. If video or audio init fails the error is logged
     * and m_Game stays empty instead of throwing, which makes Run() refuse to
     * start. The target FPS from inConfig is applied just before construction.
     * inConfig is held by reference and must outlive the Application.
     */
    template<typename ... Args>
    explicit Application(ApplicationConfig const& inConfig, Args&& ... inGameArgs)
        : m_Config(inConfig)
    {
        // Initialize the Video subsystem
        auto initResult = m_VideoSys.Initialize( inConfig.s_Title, inConfig.s_Width, inConfig.s_Height );
        if ( !initResult ) { initResult.LogError(); return; }

        // Initialize the audio subsystem
        initResult = m_AudioDev.Initialize();
        if ( !initResult ) { initResult.LogError(); return; }

        time::TargetFPS( m_Config.s_TargetFps );
        m_Game.emplace( m_VideoSys.GetRenderer(), m_AudioDev, std::forward<Args>(inGameArgs)... );
    }

    /**
     * @brief Runs the main loop until a quit event or a game-requested quit.
     *
     * Each iteration ticks the clock, rolls the input snapshot, drains the SDL
     * event queue, updates the game with the delta time and input state, then
     * renders and presents. The loop ends on an OS quit event or when the game
     * reports QuitRequested() (e.g. a state returning TransitionKind::Quit).
     * Logs an error and returns immediately if construction left m_Game empty.
     */
    void Run()
    {
        if ( !m_Game ) 
        { 
            LOG_ERROR("Application failed to initialize -- video / audio system init failed");
            return; 
        }

        m_Running = true;
        SDL_Event event;

        while ( m_Running )
        {
            GET_TIMESYSTEM.Tick();
            m_InputSys.NewFrame();

            while ( SDL_PollEvent( &event ) )
            {
                processEvent( &event );
            }

            m_Game->Update( time::DeltaTime(), m_InputSys.GetState() );

            if ( m_Game->QuitRequested() )
            {
                LOG_DEBUG("Application quit requested by game state");
                m_Running = false;
            }

            m_Game->Render( m_VideoSys.GetRenderer() );
            m_VideoSys.GetRenderer().Present();
        }
    }
};

}
