#pragma once

#include <memory>
#include <unordered_map>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include "Asset.hpp"
#include "AssetPool.hpp"
#include "FrameTable.hpp"
#include "AnimationClipSet.hpp"
#include "AnimStateMachine.hpp"
#include "ScenePrefab.hpp"

namespace asge::game::asset
{

/**
 * @brief Owns the per-asset-type `AssetPool`s and loads through a VFS.
 *
 * The one entry point for loading assets by virtual path. Each `Get*` method
 * forwards to the `AssetPool` of its asset type, which caches by virtual path
 * and only loads on a cache miss. `ResolveAssets` builds on these to load the
 * assets a whole Registry's components refer to in one pass. Does not own the
 * `VirtualFileSystem`: it must outlive the `AssetManager`.
 */
class AssetManager
{
    filesystem::VirtualFileSystem const& m_Vfs;

    AssetPool<media::Image>          m_ImagePool        { &media::Image::Load           };
    AssetPool<media::Font, int>      m_FontPool         { &media::Font::Load            };
    AssetPool<FrameTable>            m_FrameTables      { &FrameTable::Load             };
    AssetPool<media::AudioClip>      m_AudioPool        { &media::AudioClip::Load       };
    AssetPool<AnimationClipSet>      m_AnimClipSet      { &AnimationClipSet::Load       };
    AssetPool<AnimationStateMachine> m_AnimStateMachines{ &AnimationStateMachine::Load  };
    AssetPool<ScenePrefab>           m_ScenePrefabs     { &ScenePrefab::Load            };
    

    std::unordered_map<str::String, std::unique_ptr<video::ITexture>> m_TextureCache;
    std::unordered_map<media::Font const*, std::unique_ptr<video::ITexture>> m_FontAtlasCache; 

    template<typename T> using asset_ptr = std::shared_ptr<Asset<T>>;

public:
    /**
     * @brief Creates a manager whose assets are resolved through inVfs.
     *
     * inVfs is held by reference and must outlive the manager.
     */
    explicit AssetManager( filesystem::VirtualFileSystem const& inVfs ) noexcept
    : m_Vfs( inVfs )
    {}

    /**
     * @brief Copy construction is disabled.
     */
    AssetManager( AssetManager const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    AssetManager& operator=( AssetManager const& ) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    AssetManager( AssetManager&& ) = default;

    /**
     * @brief Move assignment is disabled.
     */
    AssetManager& operator=( AssetManager&& ) = delete;

    /**
     * @brief Destructor.
     *
     * Defaulted; members clean up after themselves.
     */
    ~AssetManager() = default;

    // Named GetImage (not LoadImage) -- Windows headers #define LoadImage to
    // LoadImageA/W, same reason FileIO's Copy isn't named CopyFile.
    /**
     * @brief Loads (or returns the cached) `Image` at a virtual path.
     *
     * Fails if the path doesn't resolve through the VFS or fails to decode.
     */
    [[nodiscard]] Result<asset_ptr<media::Image>> GetImage( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `Font` baked at @p inPixelHeight.
     *
     * A different @p inPixelHeight for the same path is a separate cache
     * entry.
     */
    [[nodiscard]] Result<asset_ptr<media::Font>> GetFont(str::StringCRef inVirtualPath, int inPixelHeight );

    /**
     * @brief Loads (or returns the cached) `FrameTable` at a virtual path.
     *
     * This is the meta-file a `components::Animator` clip set binds to a clip name.
     * Fails if the path doesn't resolve through the VFS or fails to parse.
     */
    [[nodiscard]] Result<asset_ptr<FrameTable>> GetFrameTable( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `AudioClip` at a virtual path.
     *
     * Fails if the path doesn't resolve through the VFS or fails to decode.
     */
    [[nodiscard]] Result<asset_ptr<media::AudioClip>> GetAudio( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `AnimationClipSet` at a virtual path.
     *
     * Only the clip set file is read: its state machine and frame tables stay
     * unresolved paths until AnimationClipSet::ResolveAssets() is called.
     * Fails if the path doesn't resolve through the VFS or fails to parse.
     */
    [[nodiscard]] Result<asset_ptr<AnimationClipSet>> GetClipSet( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `AnimationStateMachine` at a virtual path.
     *
     * Fails if the path doesn't resolve through the VFS or fails to parse
     * (see AnimStateMachineErrors). Clip sets reach their machine through
     * this same cache.
     */
    [[nodiscard]] Result<asset_ptr<AnimationStateMachine>> GetStateMachine( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `ScenePrefab` at a virtual path.
     *
     * The prefab keeps the parsed TOML, so repeated instantiation doesn't
     * re-read the file. Fails if the path doesn't resolve through the VFS,
     * fails to parse, or isn't a valid prefab (see AssetError).
     */
    [[nodiscard]] Result<asset_ptr<ScenePrefab>> GetScenePrefab( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) GPU texture for a virtual path.
     *
     * The texture is created via inRenderer on first request.
     *
     * Cached by virtual path like GetImage — later calls for the same path
     * return the same `ITexture*` rather than allocating a new one. A
     * failed image resolve/decode or texture creation is returned as-is
     * and nothing is cached, so a later call retries.
     */
    [[nodiscard]] Result<video::ITexture*> GetTexture(
        str::StringCRef inVirtualPath, video::IRenderer& inRenderer ) noexcept;

    /**
     * @brief Frees the GPU texture cached for inVirtualPath, if any.
     *
     * No-op if nothing is cached for that path; a later GetTexture() call
     * recreates it.
     */
    void UnloadTexture( str::StringCRef inVirtualPath ) noexcept;

    /**
     * @brief Drops the cached Image and GPU texture for inVirtualPath.
     *
     * Also un-resolves every Sprite in inRegistry using it. The next
     * ResolveAssets() re-reads the file from disk, so those sprites pick up
     * its new pixels instead of the stale cached ones.
     */
    void ReloadTexture( str::StringCRef inVirtualPath, ecs::Registry& inRegistry ) noexcept;

    /**
     * @brief Drops every cached asset at inVirtualPath, whatever its kind.
     *
     * Covers every asset pool. The components in inRegistry that use it
     * (directly or through another asset that binds it) are un-resolved, so
     * the next ResolveAssets() or Get*() re-reads the file from disk. A path
     * that names no cached asset is a no-op.
     */
    void ReloadAsset( str::StringCRef inVirtualPath, ecs::Registry& inRegistry ) noexcept;

    /**
     * @brief Loads (or returns the cached) GPU texture for inFont's baked glyph atlas.
     *
     * The texture is created via inRenderer on first request.
     *
     * Cached by Font address (see m_FontAtlasCache's own doc comment), not
     * a virtual path -- there is no separate atlas image file to load, the
     * atlas is already decoded in memory as part of inFont. A failed
     * texture creation is returned as-is and nothing is cached, so a later
     * call retries.
     */
    [[nodiscard]] Result<video::ITexture*> GetFontAtlasTexture(
        media::Font const& inFont, video::IRenderer& inRenderer ) noexcept;

    /**
     * @brief Deferred-loads every asset-owning component that needs (re)resolving.
     *
     * Folds over `components::SerializableComponents` and runs `Resolver<T>` on
     * each entity's T: a no-op for most, a real resolve for Sprite, Animator,
     * AudioSource and Path (see AssetResolver.hpp). A failed resolve is logged and
     * retried on the next call. Disabled entities are walked too
     * (`IncludeDisabled()`) so re-enabling one finds its assets ready.
     */
    void ResolveAssets( ecs::Registry& inRegistry, video::IRenderer& inRenderer );
};

}