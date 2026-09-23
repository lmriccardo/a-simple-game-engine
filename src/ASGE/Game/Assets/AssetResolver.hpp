#pragma once

#include <ASGE/Game/Components.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include "AssetManager.hpp"

namespace asge::game::asset
{

/**
 * @brief Customization point AssetManager::ResolveAssets dispatches to, per
 *        component type, for one entity's worth of that component.
 *
 * The primary template is a no-op — most of components::SerializableComponents
 * (Transform, Velocity, Collider, Rigidbody, Camera) have nothing to
 * deferred-load, so ResolveAssets can fold over the whole tuple uniformly
 * without every component needing its own specialization. A component that
 * does own an asset (a virtual path resolved into a live handle once a
 * renderer exists) gets one below instead.
 */
template<typename C>
struct Resolver
{
    void operator()( AssetManager&, ecs::Registry&, video::IRenderer&, C& ) const noexcept {}
};

/** @brief Resolves Sprite::m_VirtualPath into m_Texture via AssetManager::GetTexture, once. */
template<>
struct Resolver<components::Sprite>
{
    using C = components::Sprite;
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inSprite
    ) const noexcept;
};

/** @brief Resolves Animation::m_ClipPath into m_Clip via AssetManager::GetFrameTable, once. */
template<>
struct Resolver<components::Animation>
{
    using C = components::Animation;
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inAnimation
    ) const noexcept;
};

/** @brief Resolves AudioSource::m_VirtualClipPath into m_Clip via AssetManager::GetAudio, once. */
template<>
struct Resolver<components::AudioSource>
{
    using C = components::AudioSource;
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inAudioSource
    ) const noexcept;
};

/**
 * @brief Builds PathFollow::m_Path from m_Waypoints via math::
 *        CatmullRomSpline, once — skipped if m_Waypoints is empty or a
 *        path with segments already exists.
 */
template<>
struct Resolver<components::PathFollow>
{
    using C = components::PathFollow;
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inPathFollow
    ) const noexcept;
};

}
