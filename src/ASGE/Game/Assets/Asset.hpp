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
     * @brief Passkey-gated constructor; use `Create()` instead of calling
     * this directly.
     */
    Asset( PrivateTag, str::String inVirtualPath, T inValue )
    : m_VirtualPath( std::move(inVirtualPath) )
    , m_Value( std::move(inValue) )
    {}

    Asset( Asset const& ) = delete;
    Asset& operator=( Asset const& ) = delete;
    Asset( Asset&& ) = default;
    Asset& operator=( Asset&& ) = default;

    /**
     * @brief Wraps an already-loaded value with the virtual path it came
     * from, returning it `shared_ptr`-managed.
     */
    static std::shared_ptr<Asset<T>>
    Create( str::String inVirtualPath, T inValue ) noexcept
    {
        return std::make_shared<Asset<T>>(
            PrivateTag{}, std::move(inVirtualPath), std::move(inValue)
        );
    }

    /** @brief Returns the wrapped value. */
    [[nodiscard]] T const& Get() const noexcept { return m_Value; }

    /** @brief Returns the wrapped value. */
    [[nodiscard]] T& Get() noexcept { return m_Value; }

    /** @brief Returns the virtual path this asset was loaded from. */
    [[nodiscard]] str::StringView VirtualPath() const noexcept
    { return m_VirtualPath; }
};

}