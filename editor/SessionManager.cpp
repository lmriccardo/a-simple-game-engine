#include "SessionManager.hpp"

#include <vector>

#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Logger/Logger.hpp>

#include "AssetBrowser.hpp"

asge::BoolResult LoadSceneFromRealPath(
    asge::filesystem::VirtualFileSystem& inVfs, asge::game::scene::SceneManager& inSceneManager,
    std::filesystem::path const& inRealPath ) noexcept
{
    constexpr char kTempMount[] = "__open_scene__";
    auto const realDir = inRealPath.parent_path().string();

    if ( auto const r = inVfs.Mount( kTempMount, realDir ); !r ) return r;

    auto loadResult = inSceneManager.LoadScene( std::string( kTempMount ) + "/" + inRealPath.filename().string() );

    if ( auto const r = inVfs.Unmount( kTempMount, realDir ); !r ) r.LogError();

    return loadResult;
}

asge::BoolResult SaveSession(
    asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry,
    std::optional<std::filesystem::path> const& inCurrentScenePath,
    std::filesystem::path const& inPath ) noexcept
{
    asge::config::toml::TOMLBuilder builder;

    for ( auto const& mount : inVfs.ListMounts() )
    {
        auto mountTable = builder.ArrayTable( "Mount" );
        mountTable.Set( "Name", std::string( mount.m_VirtualRoot ) );
        mountTable.Set( "RealDirectory", mount.m_RealDirectory.string() );
    }

    // Every texture/animation path the Assets panel currently knows about --
    // not just what the open scene's entities reference -- so an asset
    // imported via "Load Asset..." but never assigned to anything doesn't
    // silently vanish from the panel on the next Open Session.
    for ( auto const& path : KnownTexturePaths( inRegistry ) )
    {
        auto textureTable = builder.ArrayTable( "Texture" );
        textureTable.Set( "Path", path );
    }
    for ( auto const& path : KnownAnimationPaths( inRegistry ) )
    {
        auto animationTable = builder.ArrayTable( "Animation" );
        animationTable.Set( "Path", path );
    }

    if ( inCurrentScenePath )
    {
        builder.Table( "Session" ).Set( "ScenePath", inCurrentScenePath->string() );
    }

    return builder.SaveToFile( inPath );
}

asge::BoolResult LoadSession(
    asge::filesystem::VirtualFileSystem& inVfs, asge::game::scene::SceneManager& inSceneManager,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    std::filesystem::path const& inPath,
    std::optional<std::filesystem::path>& outCurrentScenePath ) noexcept
{
    // Parse first, before touching any live state -- a bad/missing .asges
    // file must not unmount everything and leave the editor half-clobbered.
    auto parseResult = asge::config::toml::Parse( inPath );
    if ( !parseResult ) return asge::BoolResult::Err( parseResult.Error() );
    asge::config::toml::TOMLTableView const doc( parseResult.Value() );

    // ListMounts() returns a reference to the live mount table -- copy it
    // before Unmount()ing, since mutating while ranging over the original
    // would invalidate the very range being walked.
    auto const previousMounts = inVfs.ListMounts();
    for ( auto const& mount : previousMounts )
    {
        if ( auto r = inVfs.Unmount( mount.m_VirtualRoot, mount.m_RealDirectory.string() ); !r ) r.LogError();
    }

    for ( int mountIndex = 0; ; ++mountIndex )
    {
        auto getResult = doc.GetTable( "Mount", mountIndex );
        if ( !getResult )
        {
            // Running out of [[Mount]] elements is the normal, expected way
            // this loop ends -- same idiom SceneSerializer::Load uses for
            // "entity[N]".
            if ( getResult.Code() == make_error_code( asge::errors::ConfError::TomlNoSubtable ) ) break;
            return asge::BoolResult::Err( getResult.Error() );
        }

        auto const mountTable = getResult.Value();
        auto const name = mountTable.Get<std::string>( "Name", {} );
        auto const dir = mountTable.Get<std::string>( "RealDirectory", {} );

        if ( !std::filesystem::exists( dir ) )
        {
            LOG_WARNING( "Session mount \"", name, "\" -> \"", dir, "\" no longer exists, skipping" );
            continue;
        }

        if ( auto r = inVfs.Mount( name, dir ); !r ) r.LogError();
    }

    // Restore every texture/animation path the session recorded -- including
    // ones no entity in the about-to-load scene references -- before the
    // scene load below unions in whatever it uses via RegisterSceneAssets;
    // a path already present here is just a no-op insert (std::set
    // semantics), not a duplicate entry in the Assets panel.
    std::vector<std::string> textures;
    for ( int textureIndex = 0; ; ++textureIndex )
    {
        auto getResult = doc.GetTable( "Texture", textureIndex );
        if ( !getResult )
        {
            if ( getResult.Code() == make_error_code( asge::errors::ConfError::TomlNoSubtable ) ) break;
            return asge::BoolResult::Err( getResult.Error() );
        }
        textures.push_back( getResult.Value().Get<std::string>( "Path", {} ) );
    }

    std::vector<std::string> animations;
    for ( int animationIndex = 0; ; ++animationIndex )
    {
        auto getResult = doc.GetTable( "Animation", animationIndex );
        if ( !getResult )
        {
            if ( getResult.Code() == make_error_code( asge::errors::ConfError::TomlNoSubtable ) ) break;
            return asge::BoolResult::Err( getResult.Error() );
        }
        animations.push_back( getResult.Value().Get<std::string>( "Path", {} ) );
    }
    ImportAssets( textures, animations );

    // Outright replace, matching File > New / Open -- no dirty-check.
    inSceneManager.UnloadScene();
    outCurrentScenePath.reset();

    if ( doc.HasTable( "Session" ) )
    {
        auto const sessionTable = doc.GetTable( "Session" ).Value();
        if ( auto const scenePathStr = sessionTable.Get<std::string>( "ScenePath", {} ); !scenePathStr.empty() )
        {
            std::filesystem::path const scenePath = scenePathStr;
            if ( auto loadResult = LoadSceneFromRealPath( inVfs, inSceneManager, scenePath ); !loadResult )
            {
                return loadResult;
            }

            outCurrentScenePath = scenePath;
            inAssets.ResolveAssets( inSceneManager.GetRegistry(), inRenderer );
            RegisterSceneAssets( inSceneManager.GetRegistry() );
        }
    }

    return asge::BoolResult::Ok();
}
