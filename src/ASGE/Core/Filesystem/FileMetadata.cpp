#include "FileMetadata.hpp"

using namespace asge::filesystem;

bool asge::filesystem::meta::Exists(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::exists( inPath, ec );
}

bool asge::filesystem::meta::IsFile(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::is_regular_file( inPath );
}

bool asge::filesystem::meta::IsDirectory(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::is_directory(inPath, ec);
}

bool asge::filesystem::meta::IsSymlink(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::is_symlink( inPath, ec );
}

bool asge::filesystem::meta::IsSocket(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::is_socket( inPath, ec );
}

bool asge::filesystem::meta::IsEmpty(Path const &inPath) noexcept
{
    std::error_code ec;
    return std::filesystem::is_empty(inPath, ec);
}

asge::Result<FileSize> asge::filesystem::meta::GetFileSize(Path const &inPath) noexcept
{
    std::error_code ec;
    const auto size = std::filesystem::file_size( inPath, ec );
    if (ec) {
        return Result<FileSize>::Err(ec, str::ToUTF8( inPath.u8string() ));
    }

    return Result<FileSize>::Ok(size);
}

asge::Result<FileTime> asge::filesystem::meta::GetLastModified(Path const &inPath) noexcept
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time( inPath );
    if (ec) {
        return Result<FileTime>::Err(ec, str::ToUTF8( inPath.u8string() ));
    }
    return Result<FileTime>::Ok(time);
}

asge::Result<asge::str::String> asge::filesystem::meta::GetExtension(Path const &inPath) noexcept
{
    const auto ext = str::ToUTF8( inPath.extension().u8string() );

    if ( ext.empty() )
    {
        return Result<str::String>::Err( 
            std::make_error_code( std::errc::invalid_argument ),
            "Input path " + str::ToUTF8( inPath.u8string() ) + " has no extension"
        );
    }

    return Result<str::String>::Ok(ext);
}

asge::str::String asge::filesystem::meta::GetFilename(Path const& inPath) noexcept
{
    return str::ToUTF8(inPath.filename().u8string());
}

asge::str::String asge::filesystem::meta::GetStem(Path const& inPath) noexcept
{
    return str::ToUTF8(inPath.stem().u8string());
}