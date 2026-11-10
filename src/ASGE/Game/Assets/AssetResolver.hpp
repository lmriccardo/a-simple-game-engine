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

/**
 * @brief Which AssetManager pool a referenced virtual path resolves through.
 *
 * Used by AssetRefs<T> to say what kind of asset a component points at.
 */
enum class AssetKind
{
    Texture,
    AnimationClip,
    AnimationClipSet,
    AudioClip,
    Font,
};

/**
 * @brief One virtual path a component references, and what kind of asset it is.
 *
 * Produced by AssetRefs<T> and CollectAssetRefs, e.g. so a tool can list or
 * copy the files a scene depends on.
 */
struct AssetRef
{
    AssetKind   m_Kind;        // Which pool the path resolves through
    str::String m_VirtualPath; // The referenced virtual path
};

/**
 * @brief Customization point AssetManager::ResolveAssets dispatches to, per component type.
 *
 * Handles one entity's worth of that component.
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
    /**
     * @brief Does nothing: this component has no asset to resolve.
     */
    void operator()( AssetManager&, ecs::Registry&, video::IRenderer&, C& ) const noexcept {}
};

/**
 * @brief Resolves Sprite::m_VirtualPath into m_Texture via AssetManager::GetTexture.
 *
 * Re-resolves whenever m_VirtualPath differs from m_ResolvedVirtualPath — so
 * repointing it to a new path loads the new texture, and clearing it to empty
 * releases m_Texture back to nullptr — rather than resolving once and never
 * again.
 */
template<>
struct Resolver<components::Sprite>
{
    using C = components::Sprite;
    /**
     * @brief Resolves one Sprite's asset.
     *
     * See the Resolver<Sprite> comment above for when it (re)loads.
     */
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inSprite
    ) const noexcept;
};

/**
 * @brief Resolves Animator::m_ClipSetPath into m_ClipSet via AssetManager::GetClipSet.
 *
 * Re-resolves whenever m_ClipSetPath differs from the path m_ClipSet was
 * loaded from (Asset::VirtualPath), so repointing it loads the new clip set
 * and clearing it releases m_ClipSet back to null. The clip set is shared
 * between every Animator pointing at the same path, so it is resolved
 * (machine and frame tables loaded) once, by the first one to need it.
 * Whenever m_ClipSet changes, the runtime state tied to the old machine
 * (state, frame, flags, triggers) is reset and m_Started cleared so the
 * Animator enters the new machine's initial state again. On a failure the
 * Animator is left as it was.
 */
template<>
struct Resolver<components::Animator>
{
    using C = components::Animator;
    /**
     * @brief Resolves one Animator's clip set.
     *
     * See the Resolver<Animator> comment above for when it (re)loads.
     */
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inAnimator
    ) const noexcept;
};

/**
 * @brief Resolves AudioSource::m_VirtualClipPath into m_Clip via AssetManager::GetAudio.
 *
 * Re-resolves whenever m_VirtualClipPath differs from
 * m_ResolvedVirtualClipPath — so repointing it to a new clip loads it, and
 * clearing it to empty releases m_Clip back to null — rather than resolving
 * once and never again.
 */
template<>
struct Resolver<components::AudioSource>
{
    using C = components::AudioSource;
    /**
     * @brief Resolves one AudioSource's asset.
     *
     * See the Resolver<AudioSource> comment above for when it (re)loads.
     */
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inAudioSource
    ) const noexcept;
};

/**
 * @brief Builds Path::m_Path from m_Waypoints as a math::CatmullRomSpline.
 *
 * Built once — skipped if m_Waypoints is empty or a path with segments
 * already exists.
 */
template<>
struct Resolver<components::Path>
{
    using C = components::Path;
    /**
     * @brief Resolves one Path's asset.
     *
     * See the Resolver<Path> comment above for when it (re)loads.
     */
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inPath
    ) const noexcept;
};

/**
 * @brief Resolves UILabel::m_FontPath into m_Font via AssetManager::GetFont.
 *
 * Re-resolves whenever m_FontPath differs from m_ResolvedFontPath -- so
 * repointing it to a new font loads it, and clearing it to empty releases
 * m_Font back to nullptr -- rather than resolving once and never again.
 * m_Font is a non-owning pointer into AssetManager's font pool, which never
 * evicts entries, so it stays valid as long as the AssetManager does.
 */
template<>
struct Resolver<components::UILabel>
{
    using C = components::UILabel;
    /**
     * @brief Resolves one UILabel's asset.
     *
     * See the Resolver<UILabel> comment above for when it (re)loads.
     */
    void operator()(
                         AssetManager&      inAssetManager,
        [[maybe_unused]] ecs::Registry&     inRegistry,
        [[maybe_unused]] video::IRenderer&  inRenderer,
                         C&                 inLabel
    ) const noexcept;
};

/**
 * @brief Customization point CollectAssetRefs dispatches to, per component type.
 *
 * Reports which virtual path (if any) one component instance references,
 * without resolving it — Resolver<T>'s read-only sibling.
 *
 * The primary template reports nothing, same as Resolver<T>'s no-op default.
 * Every current asset-owning component has at most one path field, so this
 * returns std::optional<AssetRef> rather than a collection.
 */
template<typename C>
struct AssetRefs
{
    /**
     * @brief Reports nothing: this component references no asset.
     */
    std::optional<AssetRef> operator()( C const& ) const noexcept { return std::nullopt; }
};

/**
 * @brief Reports Sprite::m_VirtualPath as an AssetKind::Texture reference.
 *
 * Nothing is reported while the path is empty.
 */
template<>
struct AssetRefs<components::Sprite>
{
    /**
     * @brief The sprite's texture reference, or nullopt if its path is empty.
     */
    std::optional<AssetRef> operator()( components::Sprite const& inSprite ) const noexcept;
};

/**
 * @brief Reports Animator::m_ClipSetPath as an AssetKind::AnimationClipSet reference.
 *
 * Nothing is reported while the path is empty. Only the clip set file is
 * reported, not the machine and frame tables it points at.
 */
template<>
struct AssetRefs<components::Animator>
{
    /**
     * @brief The animator's clip set reference, or nullopt if its path is empty.
     */
    std::optional<AssetRef> operator()( components::Animator const& inAnimator ) const noexcept;
};

/**
 * @brief Reports AudioSource::m_VirtualClipPath as an AssetKind::AudioClip reference.
 *
 * Nothing is reported while the path is empty.
 */
template<>
struct AssetRefs<components::AudioSource>
{
    /**
     * @brief The source's audio clip reference, or nullopt if its path is empty.
     */
    std::optional<AssetRef> operator()( components::AudioSource const& inAudioSource ) const noexcept;
};

/**
 * @brief Reports UILabel::m_FontPath as an AssetKind::Font reference.
 *
 * Nothing is reported while the path is empty.
 */
template<>
struct AssetRefs<components::UILabel>
{
    /**
     * @brief The label's font reference, or nullopt if its path is empty.
     */
    std::optional<AssetRef> operator()( components::UILabel const& inLabel ) const noexcept;
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
