#pragma once

#include <tuple>

#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Components/Sprite.hpp"
#include "Components/Collider.hpp"
#include "Components/Rigidbody.hpp"

namespace asge::game::components
{

/**
 * @brief Every component type with a Serializer<T> specialization.
 *
 * The one place a scene (de)serializer needs to know about — for each
 * entity, it folds over this list checking Registry::HasComponent<T> (to
 * save) or TOMLTableView::HasTable(Serializer<T>::kTableName) (to load)
 * for each T, rather than requiring any actual reflection. Adding a new
 * serializable component means adding its type here, alongside its own
 * Serializer<T> specialization.
 */
using SerializableComponents = std::tuple<
    Transform, Velocity, Sprite, Collider, Rigidbody
>;

}
