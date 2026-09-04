#pragma once

#include <memory>
#include <vector>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Media/Font.hpp>
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
 * `GetFont`/`GetFrameTable` each forward to their own `AssetPool`, which
 * caches by virtual path (plus, for fonts, the bake pixel height) and only
 * calls the underlying `Load` on a cache miss. `ResolveAssets` builds on
 * these to deferred-load an entire Registry's worth of Sprite/Animation
 * components in one pass — see its own doc comment. Does not own `inVfs` —
 * it must outlive the `AssetManager`.
 */
class AssetManager
{
    filesystem::VirtualFileSystem const& m_Vfs;

    AssetPool<media::Image>      m_ImagePool  { &media::Image::Load };
    AssetPool<media::Font, int>  m_FontPool   { &media::Font::Load  };
    AssetPool<FrameTable>        m_FrameTables{ &FrameTable::Load   };

    // GPU textures ResolveAssets() creates from a resolved Image, keyed by
    // nothing -- Sprite::m_Texture only ever points into here, so these must
    // outlive every entity holding one (see ResolveAssets' own doc comment).
    std::vector<std::unique_ptr<video::ITexture>> m_Textures;

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
     * @brief Deferred-loads every unresolved Sprite texture and Animation
     * clip in inRegistry, in that order.
     *
     * For each `components::Sprite` with a non-empty `m_VirtualPath` but a
     * null `m_Texture`: loads (or reuses the cached) `Image` via `GetImage`,
     * creates a GPU texture from it through inRenderer, and points
     * `m_Texture` at it — the created texture is kept alive by this
     * `AssetManager` (see `m_Textures`), so it must outlive every entity
     * whose Sprite it resolved. For each `components::Animation` with a
     * non-empty `m_ClipPath` but a null `m_Clip`: loads (or reuses the
     * cached) `FrameTable` via `GetFrameTable` and points `m_Clip` at it.
     *
     * Either kind of failure (VFS resolve, decode, texture creation) is
     * logged and that entity is left unresolved — retried on the next call
     * rather than treated as fatal, since inRenderer only exists once the
     * caller has a window (unlike component construction, which can happen
     * earlier, e.g. while loading a scene).
     */
    void ResolveAssets( ecs::Registry& inRegistry, video::IRenderer& inRenderer );
};

}