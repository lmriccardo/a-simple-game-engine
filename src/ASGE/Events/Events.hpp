#pragma once

#include <cstdint>
#include <concepts>
#include "Keycode.hpp"
#include "Enums.hpp"

/* #ifdef SDL_DEFINED */
#include "Translation/SDL_TranslationUnit.hpp"
/* #endif */

namespace asge::event
{

/* Just a simple event holding its type and timestamp */
template<SystemEventType SType>
struct CommonEvent
{
    static constexpr SystemEventType STYPE = SType;

    EventType     s_Type;      // The type of the event
    std::uint64_t s_Timestamp; // Timestamp in nanoseconds
};

/* Event class just for the Quit event */
struct QuitEvent : public CommonEvent<SystemEventType::QUIT> {};

/* Handles window-releated events */
struct WindowEvent : public CommonEvent<SystemEventType::WINDOW>
{
    std::uint32_t s_WindowId; // The ID of the window
    std::int32_t  s_Data1;    // Event dependent data1
    std::int32_t  s_Data2;    // Event dependent data2
};

/* Handles keyboard-releated events */
struct KeyboardEvent : public CommonEvent<SystemEventType::KEYBOARD>
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
    inline static SystemEvent& GetUnknown() noexcept
    {
        static SystemEvent se;
        se.s_Tag = SystemEventType::UKNOWN;
        return se;
    }
};

/* Internal namespace. DO NOT USE!! */
namespace _internal
{

/**
 * @brief Process an input platfrom event
 * 
 * Given an input platform event (SDL event or other types of events)
 * returns the asge-based System Event.
 * 
 * @param inPlftEvent The input platform event
 */
SystemEvent ProcessEvent( void const* inPlftEvent ) noexcept;

};

}