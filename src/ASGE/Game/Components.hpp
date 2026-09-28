#pragma once

#include <tuple>

#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Core/ECS/Tags.hpp>

#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Components/Sprite.hpp"
#include "Components/Collider.hpp"
#include "Components/Rigidbody.hpp"
#include "Components/Animation.hpp"
#include "Components/AudioSource.hpp"
#include "Components/Camera.hpp"
#include "Components/PathFollow.hpp"
#include "Components/Hierarchy.hpp"
#include "Components/Name.hpp"
#include "Components/RenderInfo.hpp"
#include "Components/UI/Common.hpp"
#include "Components/UI/UIButton.hpp"
#include "Components/UI/UILabel.hpp"

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
    Transform, Velocity, Sprite, Collider, Rigidbody, Animation,
    AudioSource, Camera, PathFollow, Name, ecs::components::Hierarchy, 
    ecs::components::DisableTag, UIButton, RenderInfo, UIRect, Interactable, 
    UILabel
>;

/** @brief Every component type belonging to the UI subsystem. */
using UIComponents = std::tuple<UIButton, UIRect, Interactable, UILabel>;

}
