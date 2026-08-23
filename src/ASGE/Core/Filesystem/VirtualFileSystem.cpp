#include "VirtualFileSystem.hpp"

namespace
{

/**
 * @brief Normalizes a virtual mount point: backslashes -> forward slashes,
 *        then strips leading/trailing slashes and whitespace.
 *
 * "\\textures\\players\\" and "/textures/players/" and "textures/players"
 * must all resolve to the same mount point, or Resolve()'s prefix-match
 * against a differently-formatted virtual path will silently miss.
 */
asge::str::String NormalizeVirtualPath( asge::str::StringView inSv ) noexcept
{
    asge::str::String normalized( inSv );
    std::replace( normalized.begin(), normalized.end(), '\\', '/' );
    return asge::str::String( asge::str::Trim( normalized, " \t\r\n/" ) );
}

}

asge::BoolResult asge::filesystem::VirtualFileSystem::Mount(
    str::StringCRef inMntPoint, str::StringCRef inRealPath
) noexcept
{
    std::error_code ec;
    Path realPath = std::filesystem::canonical( Path( inRealPath ), ec );
    if (ec)
    {
        return BoolResult::Err( ec, inRealPath );
    }

    // Only directory can be mounted into the virtual file system
    if ( !meta::IsDirectory( realPath ) )
    {
        auto const ec = std::make_error_code( std::errc::invalid_argument );
        return BoolResult::Err( ec, "\"" + inRealPath + "\" is not a directory" );
    }

    str::String mntPoint = NormalizeVirtualPath(inMntPoint);

    // Check If the pair mntpoint and real path already exists in the mounts vector
    auto it = std::find_if( m_Mounts.begin(), m_Mounts.end(), 
        [&]( MountInfo const& m )
        { return m.m_RealDirectory == realPath && m.m_VirtualRoot == mntPoint; } 
    );
    
    if ( it != m_Mounts.end() )
    {
        return BoolResult::Err( 
            make_error_code( errors::VfsError::AlreadyMounted ),
            inMntPoint + " -> " + str::ToUTF8( realPath.u8string() )
        );
    }

    m_Mounts.emplace_back(std::move(mntPoint), std::move(realPath));
    return BoolResult::Ok();
}

asge::Result<asge::filesystem::Path> asge::filesystem::VirtualFileSystem::Resolve(
    str::StringCRef inVirtualPath) const noexcept
{
    // With no mount points yet defined there is nothing to search on
    if ( m_Mounts.empty() )
    {
        return Result<Path>::Err(make_error_code( errors::VfsError::EmptyMounts ));
    }

    str::String normalizedVirtualPath = NormalizeVirtualPath( inVirtualPath );
    str::StringView virtualPath_v = normalizedVirtualPath;

    // We discard each input virtual path containing '..'
    for ( auto const& token : str::Split( virtualPath_v, "/" ) )
    {
        if ( token == ".." )
        {
            auto const ec = std::make_error_code( std::errc::invalid_argument );
            return Result<Path>::Err( ec, "\"" + inVirtualPath + "\" contains invalid '..'." );
        }
    }

    std::error_code ec;
    for ( auto const& mountInfo : m_Mounts )
    {
        if ( !virtualPath_v.starts_with( mountInfo.m_VirtualRoot ) ) continue;
        str::StringView remainder = virtualPath_v.substr( mountInfo.m_VirtualRoot.size() );
        if ( remainder.empty() || remainder.front() != '/' )
        {
            continue; // exact mount-root match (no file component) or false-positive prefix
        }

        str::StringView dest = remainder.substr( 1 );
        Path destPath = mountInfo.m_RealDirectory / str::String(dest);
        Path destPathCanonical = std::filesystem::canonical( destPath, ec );
        if ( !ec )
        {
            return Result<Path>::Ok( destPathCanonical );
        }
    }

    return Result<Path>::Err( 
        make_error_code(errors::VfsError::NotMounted), 
        inVirtualPath 
    );
}

asge::BoolResult asge::filesystem::VirtualFileSystem::Unmount(
    str::StringCRef inMntPoint, str::StringCRef inRealPath) noexcept
{
    std::error_code ec;
    Path realPath = std::filesystem::canonical( Path( inRealPath ), ec );
    if ( ec )
    {
        return BoolResult::Err( ec, inRealPath );
    }

    str::String mntPoint = NormalizeVirtualPath( inMntPoint );

    auto it = std::find_if( m_Mounts.begin(), m_Mounts.end(),
        [&]( MountInfo const& m )
        {
            return m.m_VirtualRoot == mntPoint && m.m_RealDirectory == realPath;
        } );

    if ( it == m_Mounts.end() )
    {
        return BoolResult::Err(
            make_error_code( errors::VfsError::NotMounted ),
            str::ToUTF8( realPath.u8string() )
        );
    }

    m_Mounts.erase( it );
    return BoolResult::Ok();
}

bool asge::filesystem::VirtualFileSystem::Exists(str::StringCRef inVirtualPath) const noexcept
{
    return Resolve( inVirtualPath ).IsOk();
}

std::vector<asge::filesystem::VirtualFileSystem::MountInfo> 
const &asge::filesystem::VirtualFileSystem::ListMounts() const noexcept
{
    return m_Mounts;
}
