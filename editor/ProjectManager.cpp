#include "ProjectManager.hpp"

#include <vector>

#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Logger/Logger.hpp>

#include "AssetBrowser.hpp"

asge::BoolResult SaveProject(
    asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry,
    Project const& inProject,
    float inGridSpacing, int inTargetWidth, int inTargetHeight ) noexcept
{
    asge::config::toml::TOMLBuilder builder;

    for ( auto const& mount : inVfs.ListMounts() )
    {
        auto mountTable = builder.ArrayTable( "Mount" );
        mountTable.Set( "Name", std::string( mount.m_VirtualRoot ) );
        mountTable.Set( "RealDirectory", mount.m_RealDirectory.string() );
    }

    // Flat lists (SetArray), not the array-of-tables .asges used for these
    // -- each entry here is just one string, so there's no second field a
    // table-per-entry would actually be for.
    builder.SetArray( "Textures", KnownTexturePaths( inRegistry ) );
    builder.SetArray( "Animations", KnownAnimationPaths( inRegistry ) );
    builder.SetArray( "Audio", KnownAudioPaths( inRegistry ) );

    std::vector<std::string> scenePaths;
    scenePaths.reserve( inProject.m_Scenes.size() );
    for ( auto const& scene : inProject.m_Scenes ) scenePaths.push_back( scene.m_Path.string() );
    builder.SetArray( "Scenes", scenePaths );

    auto viewTable = builder.Table( "View" );
    viewTable.Set( "GridSpacing", inGridSpacing );
    viewTable.Set( "TargetWidth", inTargetWidth );
    viewTable.Set( "TargetHeight", inTargetHeight );

    return builder.SaveToFile( inProject.m_FilePath );
}

asge::BoolResult LoadProject(
    asge::filesystem::VirtualFileSystem& inVfs,
    std::filesystem::path const& inPath,
    Project& outProject,
    float& outGridSpacing, int& outTargetWidth, int& outTargetHeight ) noexcept
{
    // Parse first, before touching any live state -- a bad/missing
    // .asgeproject file must not unmount everything and leave the editor
    // half-clobbered.
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
            LOG_WARNING( "Project mount \"", name, "\" -> \"", dir, "\" no longer exists, skipping" );
            continue;
        }

        if ( auto r = inVfs.Mount( name, dir ); !r ) r.LogError();
    }

    ClearKnownAssets();
    ImportAssets(
        doc.GetArray<std::string>( "Textures" ),
        doc.GetArray<std::string>( "Animations" ),
        doc.GetArray<std::string>( "Audio" ) );

    outProject = Project{};
    outProject.m_FilePath = inPath;
    for ( auto const& scenePath : doc.GetArray<std::string>( "Scenes" ) )
    {
        std::filesystem::path const path = scenePath;
        outProject.m_Scenes.push_back( ProjectScene{ path.stem().string(), path, false } );
    }

    if ( doc.HasTable( "View" ) )
    {
        auto const viewTable = doc.GetTable( "View" ).Value();
        outGridSpacing = viewTable.Get( "GridSpacing", outGridSpacing );
        outTargetWidth = viewTable.Get( "TargetWidth", outTargetWidth );
        outTargetHeight = viewTable.Get( "TargetHeight", outTargetHeight );
    }

    return asge::BoolResult::Ok();
}

asge::BoolResult SaveEditorSession(
    std::optional<std::filesystem::path> const& inProjectPath,
    std::optional<std::filesystem::path> const& inActiveScenePath,
    std::filesystem::path const& inPath ) noexcept
{
    asge::config::toml::TOMLBuilder builder;

    auto sessionTable = builder.Table( "Session" );
    if ( inProjectPath ) sessionTable.Set( "ProjectPath", inProjectPath->string() );
    if ( inActiveScenePath ) sessionTable.Set( "ActiveScenePath", inActiveScenePath->string() );

    return builder.SaveToFile( inPath );
}

asge::BoolResult LoadEditorSession(
    std::filesystem::path const& inPath,
    std::optional<std::filesystem::path>& outProjectPath,
    std::optional<std::filesystem::path>& outActiveScenePath ) noexcept
{
    auto parseResult = asge::config::toml::Parse( inPath );
    if ( !parseResult ) return asge::BoolResult::Err( parseResult.Error() );
    asge::config::toml::TOMLTableView const doc( parseResult.Value() );

    if ( !doc.HasTable( "Session" ) ) return asge::BoolResult::Ok();
    auto const sessionTable = doc.GetTable( "Session" ).Value();

    if ( auto const path = sessionTable.Get<std::string>( "ProjectPath", {} ); !path.empty() )
    {
        outProjectPath = std::filesystem::path( path );
    }
    if ( auto const path = sessionTable.Get<std::string>( "ActiveScenePath", {} ); !path.empty() )
    {
        outActiveScenePath = std::filesystem::path( path );
    }

    return asge::BoolResult::Ok();
}
