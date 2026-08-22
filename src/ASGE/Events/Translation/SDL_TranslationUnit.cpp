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
    case SDL_EVENT_MOUSE_MOTION: return EventType::MOUSE_MOTION;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: return EventType::MOUSE_BUTTON_PRESSED;
    case SDL_EVENT_MOUSE_BUTTON_UP: return EventType::MOUSE_BUTTON_RELEASED;
    case SDL_EVENT_MOUSE_WHEEL: return EventType::MOUSE_WHEEL_MOTION;
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
    QuitEvent qEvent;
    qEvent.s_Type = EventType::QUIT;
    qEvent.s_Timestamp = inSdlEvent.quit.timestamp;
    *inSysEvent = SystemEvent{ std::move(qEvent) };
}

void asge::event::_internal::__processWindow(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    WindowEvent winEvent;

    __processCommonEvent_SDL( winEvent, inSdlEvent.type, inSdlEvent.window );
    
    winEvent.s_WindowId = inSdlEvent.window.windowID;
    winEvent.s_Data1 = inSdlEvent.window.data1;
    winEvent.s_Data2 = inSdlEvent.window.data2;

    *inSysEvent = SystemEvent{ std::move(winEvent) };
}

void asge::event::_internal::__processKeyboard(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    KeyboardEvent keyEvent;
    
    __processCommonEvent_SDL(keyEvent, inSdlEvent.type, inSdlEvent.key);

    keyEvent.s_WindowId = inSdlEvent.key.windowID;
    keyEvent.s_KeyboardId = inSdlEvent.key.which;
    keyEvent.s_Keycode = static_cast<input::Keycode>( inSdlEvent.key.key );
    keyEvent.s_Keymod = static_cast<input::Keymod>( inSdlEvent.key.mod );
    keyEvent.s_Down = inSdlEvent.key.down;
    keyEvent.s_Repeat = inSdlEvent.key.repeat;

    *inSysEvent = SystemEvent{ std::move(keyEvent) };
}

void asge::event::_internal::__processMouse(
    SystemEvent *inSysEvent, SDL_Event const &inSdlEvent) noexcept
{
    switch (inSdlEvent.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    {
        MouseMotionEvent moveEvent;

        __processCommonEvent_SDL(moveEvent, inSdlEvent.type, inSdlEvent.motion);

        moveEvent.s_WindowId = inSdlEvent.motion.windowID;
        moveEvent.s_MouseId = inSdlEvent.motion.which;
        moveEvent.s_Position = { inSdlEvent.motion.x, inSdlEvent.motion.y };
        moveEvent.s_Delta = { inSdlEvent.motion.xrel, inSdlEvent.motion.yrel };

        *inSysEvent = SystemEvent{ std::move(moveEvent) };
        return;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        MouseButtonEvent btnEvent;

        __processCommonEvent_SDL(btnEvent, inSdlEvent.type, inSdlEvent.button);

        btnEvent.s_WindowId = inSdlEvent.button.windowID;
        btnEvent.s_MouseId = inSdlEvent.button.which;
        btnEvent.s_Button = static_cast<input::MouseButton>( inSdlEvent.button.button );
        btnEvent.s_Down = inSdlEvent.button.down;
        btnEvent.s_Clicks = inSdlEvent.button.clicks;
        btnEvent.s_Position = { inSdlEvent.button.x, inSdlEvent.button.y };

        *inSysEvent = SystemEvent{ std::move(btnEvent) };
        return;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        MouseWheelEvent wheelEvent;

        __processCommonEvent_SDL(wheelEvent, inSdlEvent.type, inSdlEvent.wheel);

        wheelEvent.s_WindowId = inSdlEvent.wheel.windowID;
        wheelEvent.s_MouseId = inSdlEvent.wheel.which;
        wheelEvent.s_Scroll = { inSdlEvent.wheel.x, inSdlEvent.wheel.y };
        wheelEvent.s_Position = { inSdlEvent.wheel.mouse_x, inSdlEvent.wheel.mouse_y };

        *inSysEvent = SystemEvent{ std::move(wheelEvent) };
        return;
    }
    default:
        *inSysEvent = SystemEvent::GetUnknown();
        return;
    }
}

/* #endif */