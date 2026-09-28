#pragma once

namespace asge::ecs::components 
{

/**
 * @brief Marker component that hides an entity from View<Ts...> iteration.
 *
 * Registry::View() skips any entity carrying this tag unless the caller
 * opts back in with View::IncludeDisabled(), or Ts itself includes
 * DisableTag (in which case the skip is not applied at all).
 */
struct DisableTag {};

}