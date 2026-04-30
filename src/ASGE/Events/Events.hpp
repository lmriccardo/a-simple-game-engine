#pragma once

#include <cstdint>
#include <concepts>
#include <variant>
#include "Keycode.hpp"
#include "Enums.hpp"

/* #ifdef SDL_DEFINED */
#include "Translation/SDL_TranslationUnit.hpp"
/* #endif */

namespace asge::event
{

namespace _internal
{

/* Just a simple event holding its type and timestamp */
template<SystemEventType SType>
struct CommonEvent
{
    static constexpr SystemEventType tag = SType;

    EventType     s_Type;      // The type of the event
    std::uint64_t s_Timestamp; // Timestamp in nanoseconds
};

}

/* Event class for unknown events */
struct UnknownEvent : public _internal::CommonEvent<SystemEventType::UKNOWN> {};

/* Event class just for the Quit event */
struct QuitEvent : public _internal::CommonEvent<SystemEventType::QUIT> {};

/* Handles window-releated events */
struct WindowEvent : public _internal::CommonEvent<SystemEventType::WINDOW>
{
    std::uint32_t s_WindowId; // The ID of the window
    std::int32_t  s_Data1;    // Event dependent data1
    std::int32_t  s_Data2;    // Event dependent data2
};

/* Handles keyboard-releated events */
struct KeyboardEvent : public _internal::CommonEvent<SystemEventType::KEYBOARD>
{
    std::uint32_t  s_WindowId;   // The ID of the window
    std::uint32_t  s_KeyboardId; // The ID of the keyboard ( 0 if unknown )
    input::Keycode s_Keycode;    // The virtual associated keycode
    input::Keymod  s_Keymod;     // The virtual associated keymode
    bool           s_Down;       // True if the key is pressed
    bool           s_Repeat;     // True if this is a key repeat
};

namespace _internal
{

template<typename T, typename ...U>
struct is_any_same : std::disjunction<
    std::is_same<std::remove_cvref_t<T>,U>...> 
{};

template<typename T, typename ...U>
inline constexpr bool is_any_same_v = is_any_same<T, U...>::value;

template<typename T>
concept IsSystemEvent = 
    is_any_same_v<T, UnknownEvent, QuitEvent, WindowEvent, KeyboardEvent>;

/* Variant type that comprises all possible events */
using _SystemEvent = std::variant<
    UnknownEvent, QuitEvent, WindowEvent, KeyboardEvent
>;

}

/* Variant wrapper for system events. */
class SystemEvent
{
    _internal::_SystemEvent m_Event; // The actual emitted event
public:
    SystemEvent() = default;

    template<_internal::IsSystemEvent EventT>
    SystemEvent( EventT&& inEvent ) noexcept
        : m_Event{ std::forward<EventT>( inEvent ) }
    {}

    /**
     * @brief Retrieves a pointer to the specific event type.
     * @tparam EventT The event type to extract.
     * @return Pointer to EventT, or nullptr if type mismatch.
     */
    template<_internal::IsSystemEvent EventT>
    EventT const* TryGet() const noexcept
    { 
        return std::get_if<EventT>(&m_Event);
    }

    /**
     * @brief Returns a const reference to an event.
     * 
     * Unsafe version of the TryGet implementation. This returns a const
     * reference to an event. It throws an exception if the current stored
     * event does not match the requested event type.
     * 
     * @tparam EventT The event type to extract
     */
    template<_internal::IsSystemEvent EventT>
    EventT const& Get() const
    {
        return std::get<EventT>( m_Event );
    }

    /* Checks if the current system event is unknown */
    bool IsUnknown() const noexcept;

    /* Returns a const reference to an unknown event */
    static SystemEvent const& GetUnknown() noexcept;
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