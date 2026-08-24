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
 * each element's std::hash, at the indices named by @p I (see AssetPool::
 * KeyHash for how the index sequence is built).
 */
template <typename Tuple, std::size_t... I>
std::size_t HashTupleImpl( Tuple const& inTuple, std::index_sequence<I...> ) noexcept
{
    std::size_t seed = 0;
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
 * @brief A VFS-backed, path-keyed cache of `shared_ptr<Asset<T>>`, loaded via
 * a caller-supplied `Loader` on first request.
 *
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
    /** @brief Loads `T` from a resolved real path plus this pool's extra key args. */
    using Loader = std::function<Result<T>(filesystem::Path const&, KeyArgs...)>;
private:
    using Key = std::tuple<str::String, KeyArgs...>;
    /** @brief Hashes a `Key` (virtual path + KeyArgs) for `m_Assets`. */
    struct KeyHash
    {
        std::size_t operator()( Key const& inKey ) const noexcept
        {
            return detail::HashTupleImpl( inKey,
                std::make_index_sequence<1 + sizeof...(KeyArgs)>{} );
        }
    };

    std::unordered_map<Key, asset_ptr, KeyHash> m_Assets;
    Loader m_Loader;
public:
    explicit AssetPool( Loader inLoader ) noexcept
    : m_Loader( std::move(inLoader) )
    {}

    AssetPool( AssetPool const& ) = delete;
    AssetPool& operator=( AssetPool const& ) = delete;
    AssetPool( AssetPool&& ) = default;
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
};

}