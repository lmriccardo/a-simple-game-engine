#pragma once

#include <cstdint>
#include <concepts>
#include <type_traits>

namespace asge::event
{

namespace _internal
{

struct Offsets
{
    static constexpr std::uint32_t WINDOW_EVENT_START   = 0x00000010u;       // First window EventType value
    static constexpr std::uint32_t WINDOW_EVENT_END     = 0x00000100u;       // One past the last window value
    static constexpr std::uint32_t KEYBOARD_EVENT_START = WINDOW_EVENT_END;  // First keyboard EventType value
    static constexpr std::uint32_t KEYBOARD_EVENT_END   = 0x00000200u;       // One past the last keyboard value
    static constexpr std::uint32_t MOUSE_EVENT_START    = KEYBOARD_EVENT_END;// First mouse EventType value
    static constexpr std::uint32_t MOUSE_EVENT_END      = 0x00000300u;       // One past the last mouse value
};

}

enum class EventType : std::uint32_t
{
    UKNOWN,
    QUIT,                                                               // User-requested quit
    
    /* Window events */
    WINDOW_RESIZED          = _internal::Offsets::WINDOW_EVENT_START,   // Window has been resized

    /* Keyboard events */
    KEYBOARD_KEY_PRESSED    = _internal::Offsets::KEYBOARD_EVENT_START, // A key is pressed
    KEYBOARD_KEY_RELEASED,                                              // A key is released

    /* Mouse Events */
    MOUSE_MOTION            = _internal::Offsets::MOUSE_EVENT_START,    // Mouse moved
    MOUSE_BUTTON_PRESSED,                                               // Mouse button is pressed
    MOUSE_BUTTON_RELEASED,                                              // Mouse button is released
    MOUSE_WHEEL_MOTION                                                  // Mouse wheel motion
};

enum class SystemEventType
{
    UKNOWN,
    QUIT,
    WINDOW,
    KEYBOARD,
    MOUSE
};

namespace _internal
{

/**
 * @brief True if inType falls within the [Start, Stop) offset range.
 *
 * Half-open: Start is included, Stop is not, so adjacent ranges such as the
 * window and keyboard blocks never both claim the same value.
 */
template<std::uint32_t Start, std::uint32_t Stop>
inline constexpr bool IsIncluded( std::uint32_t inType ) noexcept
{
    return inType >= Start && inType < Stop;
}

/**
 * @brief True if inType's numeric value belongs to Set's EventType offset range.
 *
 * QUIT is a single value, the others are offset blocks from Offsets.
 * SystemEventType::UKNOWN (and any unlisted set) never matches.
 */
template<SystemEventType Set>
inline constexpr bool IsValidSysType( std::uint32_t inType ) noexcept
{
    switch ( Set )
    {
    case SystemEventType::QUIT: return inType == static_cast<uint32_t>( EventType::QUIT );
    case SystemEventType::WINDOW:
        return IsIncluded<Offsets::WINDOW_EVENT_START, Offsets::WINDOW_EVENT_END>(inType);
    case SystemEventType::KEYBOARD:
        return IsIncluded<Offsets::KEYBOARD_EVENT_START, Offsets::KEYBOARD_EVENT_END>(inType);
    case SystemEventType::MOUSE:
        return IsIncluded<Offsets::MOUSE_EVENT_START, Offsets::MOUSE_EVENT_END>(inType);
    default:
        return false;
    }
}

/**
 * @brief Returns SysT if inEventType belongs to it, otherwise UKNOWN.
 *
 * Thin wrapper over IsValidSysType that turns the boolean into the set tag,
 * so ToSysEventTypeImpl can fold it over a parameter pack.
 */
template<SystemEventType SysT>
inline SystemEventType CheckOne( EventType inEventType ) noexcept
{
    auto innEvent_i = static_cast<std::uint32_t>( inEventType );
    if ( IsValidSysType<SysT>( innEvent_i) ) return SysT;
    return SystemEventType::UKNOWN;
}

/**
 * @brief Tries each SysTypes in turn and returns the one that matches inEventType.
 *
 * Evaluates every set over the pack with a fold expression and keeps the
 * match, so if sets ever overlapped the last listed would win. Returns
 * UKNOWN when none match.
 */
template<SystemEventType... SysTypes>
inline SystemEventType ToSysEventTypeImpl( EventType inEventType ) noexcept
{
    SystemEventType result = SystemEventType::UKNOWN;

    // Use parameter-pack expansion to evaluate each type
    (
        [&] {
            if (CheckOne<SysTypes>( inEventType ) != SystemEventType::UKNOWN)
                result = SysTypes;
        }(),
        ...
    );
    
    return result;
}

/**
 * @brief Classifies inEventType into its owning SystemEventType.
 *
 * Checks QUIT, KEYBOARD, WINDOW and MOUSE; any EventType outside those
 * blocks (including EventType::UKNOWN) maps to SystemEventType::UKNOWN.
 */
inline SystemEventType ToSysEventType(
    EventType inEventType ) noexcept
{
    return ToSysEventTypeImpl<
        SystemEventType::QUIT,
        SystemEventType::KEYBOARD,
        SystemEventType::WINDOW,
        SystemEventType::MOUSE
    >(inEventType);
}

}

}