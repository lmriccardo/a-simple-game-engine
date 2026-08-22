#pragma once

/* #ifdef SDL_DEFINED */

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <ASGE/Events/Enums.hpp>
#include <ASGE/Input/Keycode.hpp>

namespace asge::event
{
class SystemEvent; // Forward declaration of the system event class
}

namespace asge::event::_internal
{

/** @brief Maps a raw SDL event type value to asge's EventType. */
EventType ToEventType( std::uint32_t inType ) noexcept;

/**
 * @brief Translates an SDL keycode into asge's dense internal Keycode.
 */
input::Keycode ToKeycode( SDL_Keycode inSdlKeycode ) noexcept;

/** @brief The inverse of ToKeycode. Keycode::UNKNOWN maps to SDLK_UNKNOWN. */
SDL_Keycode ToSdlKeycode( input::Keycode inKeycode ) noexcept;

/** @brief Classifies inSdlEvent and routes it to the matching __process* function. */
void ProcessEvent_SDL( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

/** @brief Fills the timestamp/type fields every asge event struct shares. */
template<typename TEvent, typename SIEvent>
inline void __processCommonEvent_SDL(
    TEvent& inSysEvent, std::uint32_t inType, SIEvent const& inSiEvent
) noexcept
{
    inSysEvent.s_Timestamp = inSiEvent.timestamp;
    inSysEvent.s_Type = ToEventType(inType);
}

/** @brief Translates an SDL quit event into inSysEvent. */
void __processQuit    ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/** @brief Translates an SDL window event into inSysEvent. */
void __processWindow  ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/** @brief Translates an SDL key up/down event into inSysEvent. */
void __processKeyboard( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/** @brief Translates an SDL motion/button/wheel event into inSysEvent. */
void __processMouse   ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

using ProcessFn = std::function<void(SystemEvent*, SDL_Event const&)>;

/** @brief Lazily-built dispatch table from SystemEventType to its SDL processing function. */
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