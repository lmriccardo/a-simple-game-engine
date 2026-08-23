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

asge::input::Keycode asge::event::_internal::ToKeycode(SDL_Keycode inSdlKeycode) noexcept
{
    switch (inSdlKeycode)
    {
    case SDLK_ESCAPE: return input::Keycode::ESCAPE;
    case SDLK_SPACE:  return input::Keycode::SPACE;
    case SDLK_A: return input::Keycode::A;
    case SDLK_B: return input::Keycode::B;
    case SDLK_C: return input::Keycode::C;
    case SDLK_D: return input::Keycode::D;
    case SDLK_E: return input::Keycode::E;
    case SDLK_F: return input::Keycode::F;
    case SDLK_G: return input::Keycode::G;
    case SDLK_H: return input::Keycode::H;
    case SDLK_I: return input::Keycode::I;
    case SDLK_J: return input::Keycode::J;
    case SDLK_K: return input::Keycode::K;
    case SDLK_L: return input::Keycode::L;
    case SDLK_M: return input::Keycode::M;
    case SDLK_N: return input::Keycode::N;
    case SDLK_O: return input::Keycode::O;
    case SDLK_P: return input::Keycode::P;
    case SDLK_Q: return input::Keycode::Q;
    case SDLK_R: return input::Keycode::R;
    case SDLK_S: return input::Keycode::S;
    case SDLK_T: return input::Keycode::T;
    case SDLK_U: return input::Keycode::U;
    case SDLK_V: return input::Keycode::V;
    case SDLK_W: return input::Keycode::W;
    case SDLK_X: return input::Keycode::X;
    case SDLK_Y: return input::Keycode::Y;
    case SDLK_Z: return input::Keycode::Z;
    case SDLK_0: return input::Keycode::NUM_0;
    case SDLK_1: return input::Keycode::NUM_1;
    case SDLK_2: return input::Keycode::NUM_2;
    case SDLK_3: return input::Keycode::NUM_3;
    case SDLK_4: return input::Keycode::NUM_4;
    case SDLK_5: return input::Keycode::NUM_5;
    case SDLK_6: return input::Keycode::NUM_6;
    case SDLK_7: return input::Keycode::NUM_7;
    case SDLK_8: return input::Keycode::NUM_8;
    case SDLK_9: return input::Keycode::NUM_9;
    case SDLK_UP: return input::Keycode::UP;
    case SDLK_DOWN: return input::Keycode::DOWN;
    case SDLK_LEFT: return input::Keycode::LEFT;
    case SDLK_RIGHT: return input::Keycode::RIGHT;
    default:
        return input::Keycode::UNKNOWN;
    }
}

SDL_Keycode asge::event::_internal::ToSdlKeycode(input::Keycode inKeycode) noexcept
{
    switch (inKeycode)
    {
    case input::Keycode::ESCAPE: return SDLK_ESCAPE;
    case input::Keycode::SPACE:  return SDLK_SPACE;
    case input::Keycode::A: return SDLK_A;
    case input::Keycode::B: return SDLK_B;
    case input::Keycode::C: return SDLK_C;
    case input::Keycode::D: return SDLK_D;
    case input::Keycode::E: return SDLK_E;
    case input::Keycode::F: return SDLK_F;
    case input::Keycode::G: return SDLK_G;
    case input::Keycode::H: return SDLK_H;
    case input::Keycode::I: return SDLK_I;
    case input::Keycode::J: return SDLK_J;
    case input::Keycode::K: return SDLK_K;
    case input::Keycode::L: return SDLK_L;
    case input::Keycode::M: return SDLK_M;
    case input::Keycode::N: return SDLK_N;
    case input::Keycode::O: return SDLK_O;
    case input::Keycode::P: return SDLK_P;
    case input::Keycode::Q: return SDLK_Q;
    case input::Keycode::R: return SDLK_R;
    case input::Keycode::S: return SDLK_S;
    case input::Keycode::T: return SDLK_T;
    case input::Keycode::U: return SDLK_U;
    case input::Keycode::V: return SDLK_V;
    case input::Keycode::W: return SDLK_W;
    case input::Keycode::X: return SDLK_X;
    case input::Keycode::Y: return SDLK_Y;
    case input::Keycode::Z: return SDLK_Z;
    case input::Keycode::NUM_0: return SDLK_0;
    case input::Keycode::NUM_1: return SDLK_1;
    case input::Keycode::NUM_2: return SDLK_2;
    case input::Keycode::NUM_3: return SDLK_3;
    case input::Keycode::NUM_4: return SDLK_4;
    case input::Keycode::NUM_5: return SDLK_5;
    case input::Keycode::NUM_6: return SDLK_6;
    case input::Keycode::NUM_7: return SDLK_7;
    case input::Keycode::NUM_8: return SDLK_8;
    case input::Keycode::NUM_9: return SDLK_9;
    case input::Keycode::UP: return SDLK_UP;
    case input::Keycode::DOWN: return SDLK_DOWN;
    case input::Keycode::LEFT: return SDLK_LEFT;
    case input::Keycode::RIGHT: return SDLK_RIGHT;
    case input::Keycode::UNKNOWN:
    case input::Keycode::COUNT:
    default:
        return SDLK_UNKNOWN;
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
    keyEvent.s_Keycode = ToKeycode( inSdlEvent.key.key );
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