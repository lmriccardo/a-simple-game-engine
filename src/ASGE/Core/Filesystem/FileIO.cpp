#include "FileIO.hpp"

asge::Result<asge::str::String> asge::filesystem::ReadText(Path const &inPath)
{
    std::ifstream file(inPath);
    std::error_code ec;

    if (!file.is_open())
    {
        ec = MakeErrorFromErrno();
        return Result<str::String>::Err( ec,str::ToUTF8( inPath.u8string() ));
    }

    str::String content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    if (file.bad())
    {
        ec = MakeErrorFromErrno();
        return Result<str::String>::Err( ec,str::ToUTF8( inPath.u8string() ));
    }

    return Result<str::String>::Ok( std::move(content) );
}

asge::BoolResult asge::filesystem::WriteText(Path const &inPath, str::String const &inContent)
{
    std::ofstream file(inPath, std::ios::trunc);
    std::error_code ec;

    if (!file.is_open())
    {
        ec = MakeErrorFromErrno();
        return BoolResult::Err( ec, str::ToUTF8( inPath.u8string() ));
    }

    file << inContent;

    if (file.bad() || file.fail())
    {
        ec = MakeErrorFromErrno();
        return BoolResult::Err( ec, str::ToUTF8( inPath.u8string() ));
    }

    return BoolResult::Ok();
}

asge::BoolResult asge::filesystem::Copy(Path const &inSource, Path const &inDestination) noexcept
{
    std::error_code ec;
    std::filesystem::copy_file(
        inSource, inDestination, std::filesystem::copy_options::overwrite_existing, ec
    );

    if (ec)
    {
        std::string detail = str::ToUTF8( inSource.u8string() ) + " -> " + str::ToUTF8( inDestination.u8string() );
        return BoolResult::Err( ec, detail );
    }

    return BoolResult::Ok();
}

// FileIO.cpp
asge::Result<std::vector<std::byte>> asge::filesystem::ReadBinary(Path const &inPath)
{
    std::ifstream file(inPath, std::ios::binary | std::ios::ate);
    std::error_code ec;

    if (!file.is_open())
    {
        ec = MakeErrorFromErrno();
        return Result<std::vector<std::byte>>::Err(ec, str::ToUTF8(inPath.u8string()));
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<std::byte> content(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(content.data()), size);

    if (file.bad())
    {
        ec = MakeErrorFromErrno();
        return Result<std::vector<std::byte>>::Err(ec, str::ToUTF8(inPath.u8string()));
    }

    return Result<std::vector<std::byte>>::Ok(std::move(content));
}
