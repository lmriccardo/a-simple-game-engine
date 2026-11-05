#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Errors.hpp>
#include "Asset.hpp"

namespace asge::game::asset
{

namespace detail
{

/**
 * @brief Combines hashes of an arbitrary tuple into one std::size_t.
 *
 * Applies boost::hash_combine's mixing formula via a fold expression over
 * each element's std::hash, at the indices named by the index sequence (see
 * AssetPool::KeyHash for how it is built).
 */
template <typename Tuple, std::size_t... I>
std::size_t HashTupleImpl( Tuple const& inTuple, std::index_sequence<I...> ) noexcept
{
    std::size_t seed = 0; // Running hash, mixed with each element
    auto combine = [&seed]( auto const& inElem )
    {
        using ElemT = std::decay_t<decltype(inElem)>;
        seed ^= std::hash<ElemT>{}( inElem ) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };
    ( combine( std::get<I>(inTuple) ), ... );
    return seed;
}

}

/**
 * @brief A VFS-backed, path-keyed cache of `shared_ptr<Asset<T>>`.
 *
 * Assets are loaded via a caller-supplied `Loader` on first request.
 * `KeyArgs` extends the cache key past the virtual path for asset types a
 * bare path doesn't fully identify — e.g. a font's bake pixel height, so
 * `"fonts/x.ttf"` at two different sizes gets two independent cache entries
 * (see `AssetManager`'s `AssetPool<graphics::Font, int>`). The key is the
 * exact virtual-path string plus `KeyArgs` as passed to `GetOrLoad` — it is
 * not canonicalized, so two differently-formatted but VFS-equivalent virtual
 * paths (e.g. a leading `/`) are cached separately.
 */
template<typename T, typename ... KeyArgs>
class AssetPool
{
public:
    using asset_ptr = std::shared_ptr<Asset<T>>;
    // Loads `T` from a resolved real path plus this pool's extra key args.
    using Loader = std::function<Result<T>(filesystem::Path const&, KeyArgs...)>;
private:
    using Key = std::tuple<str::String, KeyArgs...>;

    // Hashes a `Key` (virtual path + KeyArgs) for `m_Assets`.
    struct KeyHash
    {
        /**
         * @brief Combines the hashes of every tuple element.
         */
        std::size_t operator()( Key const& inKey ) const noexcept
        {
            return detail::HashTupleImpl( inKey,
                std::make_index_sequence<1 + sizeof...(KeyArgs)>{} );
        }
    };

    std::unordered_map<Key, asset_ptr, KeyHash> m_Assets; // Cached assets by virtual path + key args
    Loader m_Loader;                                      // Called on a cache miss to build the value
public:
    /**
     * @brief Creates an empty pool that builds missing assets with inLoader.
     */
    explicit AssetPool( Loader inLoader ) noexcept
    : m_Loader( std::move(inLoader) )
    {}

    /**
     * @brief Copy construction is disabled.
     */
    AssetPool( AssetPool const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    AssetPool& operator=( AssetPool const& ) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    AssetPool( AssetPool&& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    AssetPool& operator=( AssetPool&& ) = default;

    /**
     * @brief Returns the cached asset for this virtual path/key, loading it
     * through the VFS and `Loader` on a cache miss.
     *
     * A VFS resolve failure or a `Loader` failure is returned as-is and
     * nothing is cached for that key, so a later call retries the load.
     */
    [[nodiscard]] Result<asset_ptr> GetOrLoad(filesystem::VirtualFileSystem const& inVfs,
        str::StringCRef inVirtualPath, KeyArgs ...inArgs ) noexcept
    {
        Key key{ str::String( inVirtualPath ), inArgs... };
        if ( auto it = m_Assets.find( key ); it != m_Assets.end() )
        {
            return Result<asset_ptr>::Ok( it->second );
        }

        auto resolved = inVfs.Resolve( inVirtualPath );
        if ( !resolved ) return Result<asset_ptr>::Err( resolved.Error() );
        auto loaded = m_Loader( resolved.Value(), inArgs... );
        if ( !loaded ) return Result<asset_ptr>::Err( loaded.Error() );

        auto asset = Asset<T>::Create( str::String(inVirtualPath), std::move(loaded).Value() );
        m_Assets.emplace( std::move(key), asset );
        return Result<asset_ptr>::Ok( asset );
    }

    /**
     * @brief Removes a cached asset from the pool.
     *
     * No-op if nothing is cached for that key. Holders of the asset keep
     * their `shared_ptr`; only the pool's own reference is dropped, so the
     * next GetOrLoad() loads it again.
     */
    void Remove( str::StringCRef inVirtualPath, KeyArgs ...inArgs ) noexcept
    {
        Key key{ str::String( inVirtualPath ), inArgs... };
        if ( auto it = m_Assets.find( key ); it != m_Assets.end() )
        {
            m_Assets.erase( it );
        }
    }
};

}