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

namespace asge::game::asset
{

/**
 * @brief Owns the per-asset-type `AssetPool`s and loads through a VFS.
 *
 * The caller keeps the `VirtualFileSystem` alive. The one entry point for loading assets by virtual path: `GetImage`/
 * `GetFont`/`GetFrameTable`/`GetAudio` each forward to their own
 * `AssetPool`, which caches by virtual path (plus, for fonts, the bake
 * pixel height) and only calls the underlying `Load` on a cache miss.
 * `ResolveAssets` builds on these to deferred-load an entire Registry's
 * worth of components in one pass, dispatching per component type through
 * `Resolver<T>` — see its own doc comment. Does not own `inVfs` — it must
 * outlive the `AssetManager`.
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

    [[nodiscard]] Result<asset_ptr<AnimationClipSet>> GetClipSet( str::StringCRef inVirtualPath );
    
    [[nodiscard]] Result<asset_ptr<AnimationStateMachine>> GetStateMachine( str::StringCRef inVirtualPath );

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