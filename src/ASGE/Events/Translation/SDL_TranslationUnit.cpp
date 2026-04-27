#include "SDL_TranslationUnit.hpp"
#include <ASGE/Events/Events.hpp>

/* #ifdef SDL_DEFINED */

using namespace asge::event;

EventType asge::event::_internal::ToEventType(std::uint32_t inType) noexcept
{
    switch (inType)
    {
    case SDL_EVENT_QUIT: return EventType::QUIT;
    case SDL_EVENT_WINDOW_RESIZED: return EventType::WINDOW_RESIZED;
    case SDL_EVENT_KEY_DOWN: return EventType::KEYBOARD_KEY_PRESSED;
    case SDL_EVENT_KEY_UP: return EventType::KEYBOARD_KEY_RELEASED;
    default:
        return EventType::UKNOWN;
    }
}

void asge::event::_internal::ProcessEvent_SDL(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    EventType innType = ToEventType(inSdlEvent.type);
    *inSysEvent = SystemEvent::GetUnknown();
    if ( innType == EventType::UKNOWN ) return;

    // If the event is not unknown then we can process it
    SystemEventType sysType = ToSysEventType( innType );
    GetProcessMap().at(sysType)( inSysEvent, inSdlEvent );
    return;
}

void asge::event::_internal::__processQuit(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    inSysEvent->s_Tag = SystemEventType::QUIT;
    inSysEvent->s_Event.quit.s_Type = EventType::QUIT;
    inSysEvent->s_Event.quit.s_Timestamp = inSdlEvent.quit.timestamp;
}

void asge::event::_internal::__processWindow(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    inSysEvent->s_Tag = SystemEventType::WINDOW;
    auto& winEvent = inSysEvent->s_Event.win;

    __processCommonEvent_SDL( winEvent, inSdlEvent.type, inSdlEvent.window );
    
    winEvent.s_WindowId = inSdlEvent.window.windowID;
    winEvent.s_Data1 = inSdlEvent.window.data1;
    winEvent.s_Data2 = inSdlEvent.window.data2;
}

void asge::event::_internal::__processKeyboard(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    inSysEvent->s_Tag = SystemEventType::KEYBOARD;
    auto& keyboardEvent = inSysEvent->s_Event.key;

    __processCommonEvent_SDL(keyboardEvent, inSdlEvent.type, inSdlEvent.key);

    keyboardEvent.s_WindowId = inSdlEvent.key.windowID;
    keyboardEvent.s_KeyboardId = inSdlEvent.key.which;
    keyboardEvent.s_Keycode = static_cast<input::Keycode>( inSdlEvent.key.key );
    keyboardEvent.s_Keymod = static_cast<input::Keymod>( inSdlEvent.key.mod );
    keyboardEvent.s_Down = inSdlEvent.key.down;
    keyboardEvent.s_Repeat = inSdlEvent.key.repeat;
}

/* #endif */