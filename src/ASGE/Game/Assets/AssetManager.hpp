#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include "Asset.hpp"
#include "AssetPool.hpp"
#include "FrameTable.hpp"

namespace asge::game::asset
{

/**
 * @brief Owns the per-asset-type `AssetPool`s and resolves/loads through a
 * `VirtualFileSystem` the caller keeps alive.
 *
 * The one entry point for loading assets by virtual path: `GetImage`/
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

    AssetPool<media::Image>      m_ImagePool  { &media::Image::Load     };
    AssetPool<media::Font, int>  m_FontPool   { &media::Font::Load      };
    AssetPool<FrameTable>        m_FrameTables{ &FrameTable::Load       };
    AssetPool<media::AudioClip>  m_AudioPool  { &media::AudioClip::Load };

    // GPU textures CreateTexture() creates from a resolved Image, keyed by
    // nothing -- Sprite::m_Texture only ever points into here, so these must
    // outlive every entity holding one (see ResolveAssets' own doc comment).
    std::vector<std::unique_ptr<video::ITexture>> m_Textures;

    // GetTexture()'s cache, keyed by virtual path -- non-owning, since the
    // texture itself is already kept alive by m_Textures above.
    std::unordered_map<str::String, video::ITexture*> m_TextureCache;

    template<typename T> using asset_ptr = std::shared_ptr<Asset<T>>;

public:
    explicit AssetManager( filesystem::VirtualFileSystem const& inVfs ) noexcept
    : m_Vfs( inVfs )
    {}

    AssetManager( AssetManager const& ) = delete;
    AssetManager& operator=( AssetManager const& ) = delete;
    AssetManager( AssetManager&& ) = default;
    AssetManager& operator=( AssetManager&& ) = delete;

    ~AssetManager() = default;

    // Named GetImage (not LoadImage) -- Windows headers #define LoadImage to
    // LoadImageA/W, same reason FileIO's Copy isn't named CopyFile.
    /**
     * @brief Loads (or returns the cached) `Image` at a virtual path.
     * Fails if the path doesn't resolve through the VFS or fails to decode.
     */
    [[nodiscard]] Result<asset_ptr<media::Image>> GetImage( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `Font` baked at @p inPixelHeight.
     * A different @p inPixelHeight for the same path is a separate cache entry.
     */
    [[nodiscard]] Result<asset_ptr<media::Font>> GetFont(str::StringCRef inVirtualPath, int inPixelHeight );

    /**
     * @brief Loads (or returns the cached) `FrameTable` at a virtual path —
     * the meta-file a `components::Animation::m_ClipPath` points at.
     */
    [[nodiscard]] Result<asset_ptr<FrameTable>> GetFrameTable( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `AudioClip` at a virtual path.
     * Fails if the path doesn't resolve through the VFS or fails to decode.
     */
    [[nodiscard]] Result<asset_ptr<media::AudioClip>> GetAudio( str::StringCRef inVirtualPath );

    /**
     * @brief Creates a GPU texture from inImage via inRenderer and keeps it
     *        alive for this AssetManager's own lifetime.
     *
     * Returns a non-owning raw pointer — the texture itself lives in
     * `m_Textures` until this AssetManager is destroyed, so it (and
     * whichever `Sprite::m_Texture` ends up pointing at it) must not
     * outlive it. Returns `nullptr` if `inRenderer` fails to create one.
     */
    [[nodiscard]] video::ITexture* CreateTexture(
        video::IRenderer& inRenderer, media::Image const& inImage ) noexcept;

    /**
     * @brief Loads (or returns the cached) GPU texture for a virtual path,
     *        creating it via CreateTexture() on first request.
     *
     * Cached by virtual path like GetImage — later calls for the same path
     * return the same `ITexture*` rather than allocating a new one. A
     * failed image resolve/decode or texture creation is returned as-is
     * and nothing is cached, so a later call retries.
     */
    [[nodiscard]] Result<video::ITexture*> GetTexture(
        str::StringCRef inVirtualPath, video::IRenderer& inRenderer ) noexcept;

    /**
     * @brief Deferred-loads every still-unresolved asset-owning component
     *        in inRegistry, for every entity that has one.
     *
     * Folds over `components::SerializableComponents` and, for each type T,
     * calls `Resolver<T>{}` on every entity's T — a no-op for most
     * component types (nothing to load), and an actual resolve for the
     * ones that do own an asset (`Sprite`, `Animation`, `AudioSource`,
     * `PathFollow` — see their own `Resolver<T>` specializations in
     * AssetResolver.hpp for exactly what each one does and what it skips
     * once already resolved).
     *
     * Any resolve failure (VFS resolve, decode, texture creation) is logged
     * and that entity is left unresolved — retried on the next call rather
     * than treated as fatal, since inRenderer only exists once the caller
     * has a window (unlike component construction, which can happen
     * earlier, e.g. while loading a scene).
     */
    void ResolveAssets( ecs::Registry& inRegistry, video::IRenderer& inRenderer );
};

}