#pragma once

#include <cstdint>
#include <concepts>
#include "Keycode.hpp"
#include "Enums.hpp"

/* #ifdef SDL_DEFINED */
#include <SDL3/SDL_events.h>
/* #endif */

namespace asge::event
{

/* Just a simple event holding its type and timestamp */
template<SystemEventType SType, std::uint32_t Start, std::uint32_t Stop = Start>
struct CommonEvent
{
    static constexpr SystemEventType STYPE = SType;
    static constexpr std::uint32_t   START = Start;
    static constexpr std::uint32_t   STOP  = Stop;

    std::uint32_t s_Type;      // The type of the event
    std::uint64_t s_Timestamp; // Timestamp in nanoseconds
};

/* Event class just for the Quit event */
struct QuitEvent : public CommonEvent<
    SystemEventType::QUIT, 
    static_cast<std::uint32_t>(WindowEventType::QUIT)> 
{};

/* Handles window-releated events */
struct WindowEvent : public CommonEvent<
    SystemEventType::WINDOW,
    static_cast<uint32_t>(WindowEventType::WINDOW_RESIZED)>
{
    std::uint32_t s_WindowId; // The ID of the window
    std::int32_t  s_Data1;    // Event dependent data1
    std::int32_t  s_Data2;    // Event dependent data2
};

/* Handles keyboard-releated events */
struct KeyboardEvent : public CommonEvent<
    SystemEventType::KEYBOARD,
    static_cast<std::uint32_t>(InputEventType::KEY_PRESSED),
    static_cast<std::uint32_t>(InputEventType::KEY_RELEASED)>
{
    std::uint32_t  s_WindowId;   // The ID of the window
    std::uint32_t  s_KeyboardId; // The ID of the keyboard ( 0 if unknown )
    input::Keycode s_Keycode;    // The virtual associated keycode
    input::Keymod  s_Keymod;     // The virtual associated keymode
    bool           s_Down;       // True if the key is pressed
    bool           s_Repeat;     // True if this is a key repeat
};

/* Union of all events */
struct SystemEvent
{
    SystemEventType s_Tag; // The tag associated with the correct event class

    /* The correct event to be fill or taken */
    union _Event {

        
        QuitEvent     quit;
        WindowEvent   win;
        KeyboardEvent key;

    } s_Event;

    /* Returns an uknown system event */
    static SystemEvent GetUnknown() noexcept;
};

/* Namespace internal to the event namespace. DO NOT USE */
namespace _internal
{

template<typename TEvent>
concept IsCommonEvent = requires(TEvent t) { 
    { TEvent::START } -> std::convertible_to<std::uint32_t>; // Has a START constexpr constant 
    { TEvent::STOP  } -> std::convertible_to<std::uint32_t>; // Has a STOP  constexpr constant
    { t.s_Type      } -> std::convertible_to<std::uint32_t>; // Has a .s_Type attribute
    { t.s_Timestamp } -> std::convertible_to<std::uint64_t>; // Has a .s_Timestamp attribute
};

/**
 * @brief Process an input platfrom event
 * 
 * Given an input platform event (SDL event or other types of events)
 * returns the asge-based System Event.
 * 
 * @param inPlftEvent The input platform event
 */
SystemEvent ProcessEvent( void const* inPlftEvent ) noexcept;

/* #ifdef SDL_DEFINED */
SystemEvent ProcessEvent_SDL( SDL_Event const& inSdlEvent ) noexcept;

template<IsCommonEvent TEvent, typename SIEvent>
inline bool __processCommonEvent_SDL( 
    TEvent& inSysEvent, std::uint32_t inType, SIEvent const& inSiEvent 
) noexcept
{
    inSysEvent.s_Timestamp = inSiEvent.timestamp;
    inSysEvent.s_Type = inType;
    if ( inType < TEvent::START || inType > TEvent::STOP )
    {
        inSysEvent.s_Type = EVENT_UKNOWN;
        return false;
    }
    return true;
}

SystemEvent __processEvent_SDL_Quit    ( SDL_Event const& inSdlEvent ) noexcept;
SystemEvent __processEvent_SDL_Window  ( SDL_Event const& inSdlEvent ) noexcept;
SystemEvent __processEvent_SDL_Keyboard( SDL_Event const& inSdlEvent ) noexcept;
/* #endif */

}

}