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