#pragma once

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Graphics/Font.hpp>
#include "Asset.hpp"
#include "AssetPool.hpp"

namespace asge::game::asset
{

/**
 * @brief Owns the per-asset-type `AssetPool`s and resolves/loads through a
 * `VirtualFileSystem` the caller keeps alive.
 *
 * The one entry point for loading assets by virtual path: `LoadImage`/
 * `LoadFont` each forward to their own `AssetPool`, which caches by virtual
 * path (plus, for fonts, the bake pixel height) and only calls `Image::Load`/
 * `Font::Load` on a cache miss. Does not own `inVfs` — it must outlive the
 * `AssetManager`.
 */
class AssetManager
{
    filesystem::VirtualFileSystem const& m_Vfs;

    AssetPool<graphics::Image>      m_ImagePool{ &graphics::Image::Load };
    AssetPool<graphics::Font, int>  m_FontPool { &graphics::Font::Load  };

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

    /**
     * @brief Loads (or returns the cached) `Image` at a virtual path.
     * Fails if the path doesn't resolve through the VFS or fails to decode.
     */
    [[nodiscard]] Result<asset_ptr<graphics::Image>>
    LoadImage( str::StringCRef inVirtualPath );

    /**
     * @brief Loads (or returns the cached) `Font` baked at @p inPixelHeight.
     * A different @p inPixelHeight for the same path is a separate cache entry.
     */
    [[nodiscard]] Result<asset_ptr<graphics::Font>>
    LoadFont(str::StringCRef inVirtualPath, int inPixelHeight );
};

}