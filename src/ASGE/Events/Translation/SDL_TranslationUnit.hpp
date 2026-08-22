#pragma once

/* #ifdef SDL_DEFINED */

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <ASGE/Events/Enums.hpp>
#include <ASGE/Events/Keycode.hpp>

namespace asge::event
{
class SystemEvent; // Forward declaration of the system event class
}

namespace asge::event::_internal
{

EventType ToEventType( std::uint32_t inType ) noexcept;

void ProcessEvent_SDL( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

template<typename TEvent, typename SIEvent>
inline void __processCommonEvent_SDL( 
    TEvent& inSysEvent, std::uint32_t inType, SIEvent const& inSiEvent 
) noexcept
{
    inSysEvent.s_Timestamp = inSiEvent.timestamp;
    inSysEvent.s_Type = ToEventType(inType);
}

void __processQuit    ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
void __processWindow  ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
void __processKeyboard( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
void __processMouse   ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

using ProcessFn = std::function<void(SystemEvent*, SDL_Event const&)>;
inline std::unordered_map<SystemEventType, ProcessFn> const& GetProcessMap()
{
    static const std::unordered_map<SystemEventType, ProcessFn> map =
    {
        { SystemEventType::QUIT, &__processQuit },
        { SystemEventType::KEYBOARD, &__processKeyboard },
        { SystemEventType::WINDOW, &__processWindow },
        { SystemEventType::MOUSE, &__processMouse }
    };

    return map;
}

}

/* #endif */