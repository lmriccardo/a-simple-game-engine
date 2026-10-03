#include "AssetTree.hpp"

#include <map>

namespace
{

// The unmerged tree: every path segment is its own level.
struct RawFolder
{
    std::map<std::string, RawFolder> m_Folders;
    std::map<std::string, std::string> m_Files; // name -> full virtual path
};

AssetTreeNode MakeFolder( std::string inName, std::string inPath, RawFolder const& inRaw );

std::vector<AssetTreeNode> MakeChildren( std::string const& inPrefix, RawFolder const& inRaw )
{
    std::vector<AssetTreeNode> children;
    for ( auto const& [ name, folder ] : inRaw.m_Folders )
        children.push_back( MakeFolder( name, inPrefix.empty() ? name : inPrefix + "/" + name, folder ) );
    for ( auto const& [ name, fullPath ] : inRaw.m_Files )
        children.push_back( AssetTreeNode{ name, fullPath, false, {} } );
    return children;
}

AssetTreeNode MakeFolder( std::string inName, std::string inPath, RawFolder const& inRaw )
{
    // Merge down a chain of folders that hold nothing but one more folder.
    RawFolder const* raw = &inRaw;
    while ( raw->m_Files.empty() && raw->m_Folders.size() == 1 )
    {
        auto const& [ name, folder ] = *raw->m_Folders.begin();
        inName += "/" + name;
        inPath += "/" + name;
        raw = &folder;
    }

    return AssetTreeNode{ std::move( inName ), inPath, true, MakeChildren( inPath, *raw ) };
}

}

std::vector<AssetTreeNode> BuildAssetTree( std::set<std::string> const& inPaths )
{
    RawFolder root;
    for ( auto const& path : inPaths )
    {
        RawFolder* folder = &root;
        std::size_t start = 0;
        while ( true )
        {
            auto const slash = path.find( '/', start );
            if ( slash == std::string::npos )
            {
                if ( start < path.size() ) folder->m_Files[path.substr( start )] = path;
                break;
            }
            if ( slash > start ) folder = &folder->m_Folders[path.substr( start, slash - start )];
            start = slash + 1;
        }
    }

    return MakeChildren( {}, root );
}
