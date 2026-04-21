#include "EventType.hpp"

using namespace asge::event;

SystemEvent asge::event::SystemEvent::GetUnknown() noexcept
{
    SystemEvent se;
    se.s_Tag = SystemEventType::UKNOWN;
    return se;
}

SystemEvent asge::event::_internal::ProcessEvent(void const *inPlftEvent) noexcept
{
    // If the input platform event point is a nullptr
    // it does not know which event is declared and
    // therefore return an UKNOWN system event
    if ( inPlftEvent == nullptr ) return SystemEvent::GetUnknown();

/* #ifdef SDL_DEFINED */
    return ProcessEvent_SDL( *static_cast<SDL_Event const*>(inPlftEvent) );
/* #else */

/* #endif */
}

/* #ifdef SDL_DEFINED */

SystemEvent asge::event::_internal::ProcessEvent_SDL(SDL_Event const &inSdlEvent) noexcept
{
    std::uint32_t sdlType = inSdlEvent.type;
    if ( sdlType == SDL_EVENT_QUIT ) return __processEvent_SDL_Quit( inSdlEvent );
    if ( sdlType >= WindowEvent::START && sdlType <= WindowEvent::STOP ) 
        return __processEvent_SDL_Window( inSdlEvent );
    if ( sdlType >= KeyboardEvent::START && sdlType <= KeyboardEvent::STOP ) 
        return __processEvent_SDL_Keyboard( inSdlEvent );
    
    return SystemEvent::GetUnknown();
}

SystemEvent asge::event::_internal::__processEvent_SDL_Quit(SDL_Event const &inSdlEvent) noexcept
{
    SystemEvent se;
    se.s_Tag = SystemEventType::QUIT;
    se.s_Event.quit.s_Type = static_cast<std::uint32_t>(WindowEventType::QUIT);
    se.s_Event.quit.s_Timestamp = inSdlEvent.quit.timestamp;
    return se;
}

SystemEvent asge::event::_internal::__processEvent_SDL_Window(SDL_Event const &inSdlEvent) noexcept
{
    SystemEvent se;
    se.s_Tag = SystemEventType::WINDOW;
    auto& winEvent = se.s_Event.win;
    
    if (!__processCommonEvent_SDL(winEvent, inSdlEvent.type, inSdlEvent.window)) 
    {
        return se;
    }

    winEvent.s_WindowId = inSdlEvent.window.windowID;
    winEvent.s_Data1 = inSdlEvent.window.data1;
    winEvent.s_Data2 = inSdlEvent.window.data2;

    return se;
}

SystemEvent asge::event::_internal::__processEvent_SDL_Keyboard(SDL_Event const &inSdlEvent) noexcept
{
    SystemEvent se;
    se.s_Tag = SystemEventType::KEYBOARD;
    auto& keyboardEvent = se.s_Event.key;

    if (!__processCommonEvent_SDL(keyboardEvent, inSdlEvent.type, inSdlEvent.key)) 
    {
        return se;
    }

    keyboardEvent.s_WindowId = inSdlEvent.key.windowID;
    keyboardEvent.s_KeyboardId = inSdlEvent.key.which;
    keyboardEvent.s_Keycode = static_cast<input::Keycode>( inSdlEvent.key.key );
    keyboardEvent.s_Keymod = static_cast<input::Keymod>( inSdlEvent.key.mod );
    keyboardEvent.s_Down = inSdlEvent.key.down;
    keyboardEvent.s_Repeat = inSdlEvent.key.repeat;

    return se;
}

/* #endif */
