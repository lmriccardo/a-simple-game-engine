#pragma once

#include <memory>
#include <ASGE/Core/Strings.hpp>

namespace asge::game::asset
{

/**
 * @brief A loaded value paired with the virtual path it was loaded from.
 *
 * The asset-handle type ECS components and scenes hold instead of a
 * hardcoded path: wraps whatever a loader (e.g. `Image::Load`, `Font::Load`)
 * produced, alongside the virtual path (see `filesystem::VirtualFileSystem`)
 * it was resolved from. Always `shared_ptr`-managed (see `Create()`) so
 * `enable_shared_from_this` is safe to use.
 */
template<typename T>
class Asset final
{
    str::String m_VirtualPath; // Virtual path the asset was resolved from
    T           m_Value;       // The loaded value itself

    /**
     * @brief Passkey restricting construction to `Create()`.
     *
     * Only `Asset` itself can name/construct a `PrivateTag`, so the
     * tag-taking constructor below is effectively unreachable from outside
     * the class even though it has to stay `public` for `make_shared` to
     * call it — every `Asset` ends up `shared_ptr`-managed.
     */
    struct PrivateTag { explicit PrivateTag() = default; };
public:
    using value_type = T;

    /**
     * @brief Passkey-gated constructor.
     *
     * Use `Create()` instead of calling this directly; it is public only so
     * `make_shared` can reach it.
     */
    Asset( PrivateTag, str::String inVirtualPath, T inValue )
    : m_VirtualPath( std::move(inVirtualPath) )
    , m_Value( std::move(inValue) )
    {}

    /**
     * @brief Copy construction is disabled.
     */
    Asset( Asset const& ) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    Asset& operator=( Asset const& ) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    Asset( Asset&& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    Asset& operator=( Asset&& ) = default;

    /**
     * @brief Wraps an already-loaded value with the virtual path it came from.
     *
     * Returns it `shared_ptr`-managed, which is the only way to obtain an
     * Asset since the constructor is passkey-gated.
     */
    static std::shared_ptr<Asset<T>>
    Create( str::String inVirtualPath, T inValue ) noexcept
    {
        return std::make_shared<Asset<T>>(
            PrivateTag{}, std::move(inVirtualPath), std::move(inValue)
        );
    }

    /**
     * @brief Returns the wrapped value.
     *
     * The reference lives as long as the Asset does.
     */
    [[nodiscard]] T const& Get() const noexcept { return m_Value; }

    /**
     * @brief Returns the wrapped value, mutable.
     *
     * The asset is shared, so a change is seen by every holder.
     */
    [[nodiscard]] T& Get() noexcept { return m_Value; }

    /**
     * @brief Returns the virtual path this asset was loaded from.
     *
     * The view points into the Asset and is valid as long as it is.
     */
    [[nodiscard]] str::StringView VirtualPath() const noexcept
    { return m_VirtualPath; }
};

}