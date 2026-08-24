#pragma once

#include <ASGE/Core/Graphics/Image.hpp>
#include <ASGE/Core/Graphics/Font.hpp>

namespace asge::game::asset
{

/**
 * @brief Runtime tag identifying which concrete type an `Asset<T>` wraps.
 *
 * Lets code holding only an `AssetType` value (not the template parameter
 * itself) branch on what kind of asset it's looking at — e.g. a future
 * heterogeneous asset registry/cache keyed by this tag.
 */
enum class AssetType { Unknown, Image, Font };

/**
 * @brief Maps a loaded-asset type `T` to its `AssetType` tag at compile time.
 *
 * Specialized per supported asset type below; the unspecialized primary
 * template resolves to `AssetType::Unknown` for any type not yet wired up.
 */
template<typename T> struct GetAssetType
{ static constexpr AssetType type = AssetType::Unknown; };

// ---------------------------------------------------------------------------
// DEFINE_ASSET_TYPE — specializes GetAssetType<Type> to report
// AssetType::Tag, so adding a new supported asset type doesn't need a
// hand-written specialization struct.
// ---------------------------------------------------------------------------
#define DEFINE_ASSET_TYPE(Type, Tag)                        \
    template<>                                               \
    struct GetAssetType<Type>                                 \
    {                                                          \
        static constexpr AssetType type = AssetType::Tag;      \
    }

DEFINE_ASSET_TYPE(graphics::Image, Image);
DEFINE_ASSET_TYPE(graphics::Font, Font);

#undef DEFINE_ASSET_TYPE

}