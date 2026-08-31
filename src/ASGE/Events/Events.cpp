#include "Events.hpp"

using namespace asge::event;

SystemEvent asge::event::_internal::ProcessEvent(void const *inPlftEvent) noexcept
{
    // If the input platform event point is a nullptr
    // it does not know which event is declared and
    // therefore return an UKNOWN system event
    if ( inPlftEvent == nullptr ) return SystemEvent::GetUnknown();

    SystemEvent rEvent;

/* #ifdef SDL_DEFINED */
    ProcessEvent_SDL( &rEvent, *static_cast<SDL_Event const*>(inPlftEvent) );
/* #else */

/* #endif */

    return rEvent;
}

bool asge::event::SystemEvent::IsUnknown() const noexcept
{
    return std::holds_alternative<UnknownEvent>( m_Event );
}

SystemEvent const &asge::event::SystemEvent::GetUnknown() noexcept
{
    static SystemEvent se{ UnknownEvent{} };
    return se;
}