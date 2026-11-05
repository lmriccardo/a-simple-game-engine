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

/**
 * @brief Maps a raw SDL event type value to asge's EventType.
 *
 * Only the handful of SDL events the engine understands are mapped (quit,
 * window resize, key up/down, mouse motion/button/wheel). Everything else
 * returns EventType::UKNOWN, which ProcessEvent_SDL treats as "drop it".
 */
EventType ToEventType( std::uint32_t inType ) noexcept;

/**
 * @brief Translates an SDL keycode into asge's dense internal Keycode.
 *
 * Covers letters, digits, arrows, Escape, Space and Enter (both Return and
 * keypad Enter map to Keycode::ENTER). Unmapped keys give Keycode::UNKNOWN.
 */
input::Keycode ToKeycode( SDL_Keycode inSdlKeycode ) noexcept;

/**
 * @brief The inverse of ToKeycode.
 *
 * Keycode::UNKNOWN and Keycode::COUNT map to SDLK_UNKNOWN. Not a perfect
 * round trip: keypad Enter comes back as plain SDLK_RETURN.
 */
SDL_Keycode ToSdlKeycode( input::Keycode inKeycode ) noexcept;

/**
 * @brief Classifies inSdlEvent and routes it to the matching __process* function.
 *
 * Always overwrites *inSysEvent: it is reset to the unknown event first, so
 * an SDL event the engine does not map leaves it unknown rather than stale.
 * Dispatch goes through GetProcessMap() keyed by SystemEventType.
 */
void ProcessEvent_SDL( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

/**
 * @brief Fills the timestamp/type fields every asge event struct shares.
 *
 * Works for any asge event deriving from CommonEvent paired with the SDL
 * sub-event (key, motion, ...) that carries a timestamp field. inType is
 * the SDL event type, converted through ToEventType.
 */
template<typename TEvent, typename SIEvent>
inline void __processCommonEvent_SDL(
    TEvent& inSysEvent, std::uint32_t inType, SIEvent const& inSiEvent
) noexcept
{
    inSysEvent.s_Timestamp = inSiEvent.timestamp;
    inSysEvent.s_Type = ToEventType(inType);
}

/**
 * @brief Translates an SDL quit event into inSysEvent.
 *
 * Produces a QuitEvent carrying the SDL timestamp. Replaces the contents
 * of *inSysEvent.
 */
void __processQuit    ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/**
 * @brief Translates an SDL window event into inSysEvent.
 *
 * Produces a WindowEvent with the window id and SDL's two data fields
 * (for a resize, the new width and height). Replaces *inSysEvent.
 */
void __processWindow  ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/**
 * @brief Translates an SDL key up/down event into inSysEvent.
 *
 * Produces a KeyboardEvent with the translated Keycode, the modifier mask,
 * and the down/repeat flags. Replaces *inSysEvent.
 */
void __processKeyboard( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;
/**
 * @brief Translates an SDL motion/button/wheel event into inSysEvent.
 *
 * Picks MouseMotionEvent, MouseButtonEvent or MouseWheelEvent from the SDL
 * event type. Any other type leaves *inSysEvent set to the unknown event.
 */
void __processMouse   ( SystemEvent* inSysEvent, SDL_Event const& inSdlEvent ) noexcept;

using ProcessFn = std::function<void(SystemEvent*, SDL_Event const&)>;

/**
 * @brief Lazily-built dispatch table from SystemEventType to its SDL processing function.
 *
 * Built once on first use (function-local static), so it is thread-safe to
 * initialise. It has no UKNOWN entry: callers must filter unknown types
 * before indexing it with at().
 */
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