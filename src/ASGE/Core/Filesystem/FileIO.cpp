#include "FileIO.hpp"

asge::Result<asge::str::String> asge::filesystem::ReadText(Path const &inPath)
{
    std::ifstream file(inPath);
    std::error_code ec;

    if (!file.is_open())
    {
        ec = std::error_code( errno, std::generic_category() );
        return Result<str::String>::Err( ec,str::ToUTF8( inPath.u8string() ));
    }

    str::String content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    if (file.bad())
    {
        ec = std::error_code( errno, std::generic_category() );
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
        ec = std::error_code( errno, std::generic_category() );
        return BoolResult::Err( ec, str::ToUTF8( inPath.u8string() ));
    }

    file << inContent;

    if (file.bad() || file.fail())
    {
        ec = std::error_code( errno, std::generic_category() );
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