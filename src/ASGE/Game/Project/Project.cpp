#include "Project.hpp"

#include <ASGE/Core/Configuration/TOML_Builder.hpp>

namespace
{

asge::filesystem::Path Resolve( asge::filesystem::Path const& inBase, std::string const& inPath )
{
    asge::filesystem::Path const path = inPath;
    return path.is_absolute() ? path : ( inBase / path ).lexically_normal();
}

}

asge::Result<asge::game::project::ProjectData> asge::game::project::LoadProjectFile(
    filesystem::Path const& inPath ) noexcept
{
    auto parseResult = config::toml::Parse( inPath );
    if ( !parseResult ) return Result<ProjectData>::Err( parseResult.Error() );
    config::toml::TOMLTableView const doc( parseResult.Value() );

    ProjectData data;
    data.m_FilePath = inPath;
    auto const base = inPath.parent_path();

    for ( int index = 0; ; ++index )
    {
        auto getResult = doc.GetTable( "Mount", index );
        if ( !getResult )
        {
            if ( getResult.Code() == make_error_code( errors::ConfError::TomlNoSubtable ) ) break;
            return Result<ProjectData>::Err( getResult.Error() );
        }

        auto const table = getResult.Value();
        data.m_Mounts.push_back( Mount{
            table.Get<std::string>( "Name", {} ),
            Resolve( base, table.Get<std::string>( "RealDirectory", {} ) ) } );
    }

    for ( auto const& scene : doc.GetArray<std::string>( "Scenes" ) )
    {
        data.m_Scenes.push_back( Resolve( base, scene ) );
    }

    if ( auto const main = doc.Get<std::string>( "MainScene", {} ); !main.empty() )
    {
        data.m_MainScene = Resolve( base, main );
    }

    if ( doc.HasTable( "View" ) )
    {
        auto const view = doc.GetTable( "View" ).Value();
        data.m_TargetWidth = view.Get( "TargetWidth", 0 );
        data.m_TargetHeight = view.Get( "TargetHeight", 0 );
        data.m_TargetFps = view.Get( "TargetFPS", 0 );
    }

    return Result<ProjectData>::Ok( std::move( data ) );
}
