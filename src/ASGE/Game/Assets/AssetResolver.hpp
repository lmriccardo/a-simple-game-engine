#pragma once

#include <optional>
#include <vector>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Strings.hpp>

#include "AssetManager.hpp"

namespace asge::game::asset
{

/** @brief Which AssetManager pool a virtual path referenced by AssetRefs<T> would resolve through. */
enum class AssetKind
{
    Texture,
    AnimationClip,
    AudioClip,
};

/** @brief One virtual path a component references, and what kind of asset it is. */
struct AssetRef
{
    AssetKind   m_Kind;
    str::String m_VirtualPath;
};

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

/**
 * @brief Customization point CollectAssetRefs dispatches to, per component
 *        type, to report which virtual path (if any) one component instance
 *        references, without resolving it — Resolver<T>'s read-only sibling.
 *
 * The primary template reports nothing, same as Resolver<T>'s no-op default.
 * Every current asset-owning component has at most one path field, so this
 * returns std::optional<AssetRef> rather than a collection.
 */
template<typename C>
struct AssetRefs
{
    std::optional<AssetRef> operator()( C const& ) const noexcept { return std::nullopt; }
};

/** @brief Reports Sprite::m_VirtualPath as an AssetKind::Texture reference, if set. */
template<>
struct AssetRefs<components::Sprite>
{
    std::optional<AssetRef> operator()( components::Sprite const& inSprite ) const noexcept;
};

/** @brief Reports Animation::m_ClipPath as an AssetKind::AnimationClip reference, if set. */
template<>
struct AssetRefs<components::Animation>
{
    std::optional<AssetRef> operator()( components::Animation const& inAnimation ) const noexcept;
};

/** @brief Reports AudioSource::m_VirtualClipPath as an AssetKind::AudioClip reference, if set. */
template<>
struct AssetRefs<components::AudioSource>
{
    std::optional<AssetRef> operator()( components::AudioSource const& inAudioSource ) const noexcept;
};

/**
 * @brief Every asset reference held by any component in inRegistry.
 *
 * Folds over components::SerializableComponents the same way ResolveAssets
 * does, dispatching each entity's each component through AssetRefs<T>
 * instead of Resolver<T> — read-only, so nothing is loaded or created.
 */
[[nodiscard]] std::vector<AssetRef> CollectAssetRefs( ecs::Registry const& inRegistry );

}
