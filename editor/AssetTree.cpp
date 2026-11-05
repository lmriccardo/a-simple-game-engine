#include "AssetTree.hpp"

#include <set>

FolderListing ListFolder( std::set<std::string> const& inPaths, std::string const& inFolder )
{
    std::string const prefix = inFolder.empty() ? std::string{} : inFolder + "/";

    std::set<std::string> folders;
    std::set<std::string> files;
    for ( auto const& path : inPaths )
    {
        if ( path.compare( 0, prefix.size(), prefix ) != 0 ) continue;

        std::string const rest = path.substr( prefix.size() );
        auto const slash = rest.find( '/' );
        if ( slash == std::string::npos )
        {
            if ( !rest.empty() ) files.insert( rest );
        }
        else if ( slash > 0 )
        {
            folders.insert( rest.substr( 0, slash ) );
        }
    }

    return FolderListing{
        std::vector<std::string>( folders.begin(), folders.end() ),
        std::vector<std::string>( files.begin(), files.end() ) };
}
