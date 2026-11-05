#pragma once

#include <cstdint>
#include <concepts>
#include <variant>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Input/Keycode.hpp>
#include <ASGE/Input/MouseButton.hpp>
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
    static constexpr SystemEventType tag = SType; // Compile-time tag of the owning event set

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

/* Handles mouse-movement events */
struct MouseMotionEvent : public _internal::CommonEvent<SystemEventType::MOUSE>
{
    std::uint32_t s_WindowId; // The ID of the window
    std::uint32_t s_MouseId;  // The ID of the mouse ( 0 if unknown, or touch-emulated )
    math::Float2  s_Position; // Position, relative to the window
    math::Float2  s_Delta;    // Relative motion since the last motion event
};

/* Handles mouse-button-press/release events */
struct MouseButtonEvent : public _internal::CommonEvent<SystemEventType::MOUSE>
{
    std::uint32_t      s_WindowId; // The ID of the window
    std::uint32_t      s_MouseId;  // The ID of the mouse ( 0 if unknown, or touch-emulated )
    input::MouseButton s_Button;   // The button this event refers to
    bool               s_Down;     // True if the button is pressed
    std::uint8_t       s_Clicks;   // 1 for single-click, 2 for double-click, etc.
    math::Float2       s_Position; // Position, relative to the window
};

/* Handles mouse-wheel/scroll events */
struct MouseWheelEvent : public _internal::CommonEvent<SystemEventType::MOUSE>
{
    std::uint32_t s_WindowId; // The ID of the window
    std::uint32_t s_MouseId;  // The ID of the mouse ( 0 if unknown, or touch-emulated )
    math::Float2  s_Scroll;   // Amount scrolled ( +x right, +y away from the user )
    math::Float2  s_Position; // Mouse position, relative to the window
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
    is_any_same_v<T, UnknownEvent, QuitEvent, WindowEvent, KeyboardEvent,
        MouseMotionEvent, MouseButtonEvent, MouseWheelEvent>;

/* Variant type that comprises all possible events */
using _SystemEvent = std::variant<
    UnknownEvent, QuitEvent, WindowEvent, KeyboardEvent,
    MouseMotionEvent, MouseButtonEvent, MouseWheelEvent
>;

}

/* Variant wrapper for system events. */
class SystemEvent
{
    _internal::_SystemEvent m_Event; // The actual emitted event
public:
    /**
     * @brief Creates an empty event holding an UnknownEvent.
     *
     * The variant's first alternative is default-constructed, so IsUnknown()
     * is true until a real event is assigned.
     */
    SystemEvent() = default;

    /**
     * @brief Wraps one concrete event (Quit, Window, Keyboard, Mouse*...).
     *
     * Accepts any type satisfying IsSystemEvent, by forwarding reference,
     * and stores it in the variant. Non-event types fail the constraint at
     * compile time, so there is no implicit conversion from arbitrary data.
     */
    template<_internal::IsSystemEvent EventT>
    SystemEvent( EventT&& inEvent ) noexcept
        : m_Event{ std::forward<EventT>( inEvent ) }
    {}

    /**
     * @brief Retrieves a pointer to the stored event if it is an EventT.
     *
     * The non-throwing accessor: returns nullptr when the variant holds a
     * different event type, so it doubles as the type test in
     * `if ( auto const* e = ev.TryGet<KeyboardEvent>() )`. The pointer
     * refers into this SystemEvent and dies with it.
     */
    template<_internal::IsSystemEvent EventT>
    EventT const* TryGet() const noexcept
    { 
        return std::get_if<EventT>(&m_Event);
    }

    /**
     * @brief Returns the stored event as EventT, throwing on a type mismatch.
     *
     * Throws std::bad_variant_access if the variant holds another event type,
     * so only use it once the type is already known. Prefer TryGet when the
     * event kind is not guaranteed.
     */
    template<_internal::IsSystemEvent EventT>
    EventT const& Get() const
    {
        return std::get<EventT>( m_Event );
    }

    /**
     * @brief True if this SystemEvent holds no recognized event.
     *
     * True for a default-constructed event and for platform events that
     * the translation layer could not classify.
     */
    bool IsUnknown() const noexcept;

    /**
     * @brief Shared instance of the unknown/no-op event.
     *
     * Returned by reference to a single static, so callers can compare
     * or hand it out without constructing one. It must not be modified.
     */
    static SystemEvent const& GetUnknown() noexcept;
};

/* Internal namespace. DO NOT USE!! */
namespace _internal
{

/**
 * @brief Translates a platform event (e.g. SDL) into an asge SystemEvent.
 *
 * inPlftEvent points at the platform's native event (an SDL_Event for the
 * SDL backend) and may be nullptr, which yields the unknown event. Types the
 * backend does not map are also returned as unknown instead of failing.
 */
SystemEvent ProcessEvent( void const* inPlftEvent ) noexcept;

};

}