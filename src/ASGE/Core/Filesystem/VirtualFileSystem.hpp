#pragma once

#include <vector>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>
#include "FileData.hpp"
#include "FileMetadata.hpp"

namespace asge::filesystem
{

/**
 * @brief Maps virtual asset paths onto real on-disk directories.
 *
 * A mount table + resolver sitting on top of `std::filesystem`: `Mount()` binds
 * a virtual root (e.g. `"textures"`) to a real directory, and `Resolve()` turns
 * a virtual path (e.g. `"textures/hero.png"`) into the real path of the first
 * registered mount whose target directory actually contains that file. It does
 * no I/O of its own beyond that lookup — callers still read the resolved path
 * through the normal `FileIO` functions.
 */
class VirtualFileSystem
{
    /** @brief One virtual-root-to-real-directory binding in the mount table. */
    struct MountInfo
    {
        str::String m_VirtualRoot;  // Normalized virtual mount point (no leading/trailing slashes)
        Path m_RealDirectory;       // Canonical real directory the mount point resolves to
    };
    std::vector<MountInfo> m_Mounts{};
public:
    /** @brief A virtual path split into its root (first segment) and the remainder after it. */
    struct SplitPath
    {
        str::String m_Root; // The path's first segment, e.g. "textures"
        str::String m_Rest; // Everything after the root's slash, or empty if there wasn't one
    };

    VirtualFileSystem() = default;

    VirtualFileSystem( VirtualFileSystem const& ) = delete;
    VirtualFileSystem( VirtualFileSystem && ) = default;
    VirtualFileSystem& operator=( VirtualFileSystem const& ) = delete;
    VirtualFileSystem& operator=( VirtualFileSystem && ) = default;

    ~VirtualFileSystem() = default;

    /**
     * @brief Binds a virtual mount point to a real directory.
     *
     * @p inRealPath must already exist and be a directory; it is canonicalized
     * and @p inMntPoint normalized before being recorded, so equivalent forms
     * (slashes, `.`/`..`, relative vs. absolute) collapse onto the same entry
     * instead of registering a duplicate mount.
     */
    [[nodiscard]] BoolResult Mount( str::StringCRef inMntPoint, str::StringCRef inRealPath ) noexcept;

    /**
     * @brief Resolves a virtual path to the real path it currently maps to.
     *
     * Tries every mount whose virtual root prefixes @p inVirtualPath, in the
     * order they were registered, and returns the real path of the first one
     * whose target directory actually contains the remainder. A virtual path
     * containing a `..` segment is rejected before any mount is tried.
     */
    [[nodiscard]] Result<Path> Resolve( str::StringCRef inVirtualPath ) const noexcept;

    /**
     * @brief Removes a previously registered mount-point/real-path binding.
     */
    [[nodiscard]] BoolResult Unmount( str::StringCRef inMntPoint, str::StringCRef inRealPath ) noexcept;

    /**
     * @brief Checks whether a virtual path currently resolves to a real file.
     *
     * Equivalent to `Resolve(inVirtualPath).IsOk()`.
     */
    [[nodiscard]] bool Exists( str::StringCRef inVirtualPath ) const noexcept;

    /** @brief Returns the currently registered mounts, in resolution order. */
    [[nodiscard]] std::vector<MountInfo> const& ListMounts() const noexcept;

    /**
     * @brief Splits a virtual path into its root and the remainder after it —
     *        the same split Resolve() does internally to find a matching
     *        mount, exposed for callers that just need the root.
     *
     * Normalizes @p inVirtualPath first, same as Mount()/Resolve(), so
     * equivalent forms (slashes, leading/trailing whitespace) split the same.
     */
    [[nodiscard]] static SplitPath SplitRoot( str::StringCRef inVirtualPath ) noexcept;

    /**
     * @brief Checks whether @p inRoot is currently a registered mount point.
     * @p inRoot is normalized the same way as a Mount() call's mount point.
     */
    [[nodiscard]] bool IsMounted( str::StringCRef inRoot ) const noexcept;

    /**
     * @brief Finds a mount whose real directory contains @p inRealPath and
     *        returns the corresponding virtual path.
     *
     * Tries every mount in registration order, like Resolve() does in
     * reverse; a path that only reaches a mount's directory by escaping it
     * via `..` doesn't count as contained. @p inRealPath does not need to
     * exist. Returns a VfsError::NotMounted error if no mount contains it.
     */
    [[nodiscard]] Result<str::String> ToVirtualPath( Path const& inRealPath ) const noexcept;
};

}