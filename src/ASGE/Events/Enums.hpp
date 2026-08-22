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
    static constexpr std::uint32_t WINDOW_EVENT_START   = 0x00000010u;
    static constexpr std::uint32_t WINDOW_EVENT_END     = 0x00000100u;
    static constexpr std::uint32_t KEYBOARD_EVENT_START = WINDOW_EVENT_END;
    static constexpr std::uint32_t KEYBOARD_EVENT_END   = 0x00000200u;
    static constexpr std::uint32_t MOUSE_EVENT_START    = KEYBOARD_EVENT_END;
    static constexpr std::uint32_t MOUSE_EVENT_END      = 0x00000300u;
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

template<std::uint32_t Start, std::uint32_t Stop>
inline constexpr bool IsIncluded( std::uint32_t inType ) noexcept
{
    return inType >= Start && inType < Stop;
}

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

template<SystemEventType SysT>
inline SystemEventType CheckOne( EventType inEventType ) noexcept
{
    auto innEvent_i = static_cast<std::uint32_t>( inEventType );
    if ( IsValidSysType<SysT>( innEvent_i) ) return SysT;
    return SystemEventType::UKNOWN;
}

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